#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    //! \brief Sends an IPv4 datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination address).

    //! Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next hop
    //! ("Sending" is accomplished by pushing the frame onto the frames_out queue.)
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    // If the destination Ethernet address is already known, send it right away. Create
    // an Ethernet frame (with type = EthernetHeader::TYPE IPv4), set the payload to
    // be the serialized datagram, and set the source and destination addresses
    if (ip_mac_map_.find(next_hop_ip) != ip_mac_map_.end() && ip_mac_map_[next_hop_ip].ttl_ > 0) {
        EthernetFrame ethernet_frame;
        ethernet_frame.header().type = EthernetHeader::TYPE_IPv4;
        ethernet_frame.header().src = _ethernet_address;
        ethernet_frame.header().dst = ip_mac_map_[next_hop_ip].ethernet_address_;
        ethernet_frame.payload() = dgram.serialize();

        // 发送
        frames_out().push(ethernet_frame);

    } else {
        EthernetFrame ethernet_frame;
        ethernet_frame.header().type = EthernetHeader::TYPE_ARP;
        ethernet_frame.header().src = _ethernet_address;
        ethernet_frame.header().dst = ETHERNET_BROADCAST;

        ARPMessage message;
        message.opcode = ARPMessage::OPCODE_REQUEST;
        message.sender_ethernet_address = _ethernet_address;
        message.sender_ip_address = _ip_address.ipv4_numeric();
        message.target_ip_address = next_hop_ip;
        // 广播出去需要得知的mac地址
        message.target_ethernet_address = {};

        ethernet_frame.payload() = message.serialize();

        if (last_second_map_.find(next_hop_ip) == last_second_map_.end()) {
            frames_out().push(ethernet_frame);
            last_second_map_[next_hop_ip] = 5000;
        }

        send_queue_.push(dgram);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    //! If type is IPv4, returns the datagram.
    //! If type is ARP request, learn a  mapping from the "sender" fields, and send an ARP reply.
    //! If type is ARP reply, learn a mapping from the "sender" fields.
    // 发错人了，丢弃
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return {};
    }

    uint32_t type = frame.header().type;
    if (type == frame.header().TYPE_IPv4) {
        // 将datagram解析出来
        InternetDatagram datagram;
        ParseResult res = datagram.parse(frame.payload());

        if (res == ParseResult::NoError) {
            return datagram;
        }

    } else if (type == frame.header().TYPE_ARP) {
        // 解析ARP数据报文
        ARPMessage message;
        ParseResult res = message.parse(frame.payload().buffers().front());

        if (res == ParseResult::NoError) {
            if (frame.header().dst == ETHERNET_BROADCAST || message.opcode == ARPMessage::OPCODE_REQUEST) {
                // 丢弃不必要的报文，注意这时候只能靠ip判断
                if (message.target_ip_address != _ip_address.ipv4_numeric()) {
                    return {};
                }

                // 收到对方发送的广播，为对方填充queue
                EthernetFrame temp_frame;
                temp_frame.header().type = EthernetHeader::TYPE_ARP;
                temp_frame.header().src = _ethernet_address;
                temp_frame.header().dst = frame.header().src;

                ARPMessage temp_message;
                temp_message.sender_ethernet_address = _ethernet_address;
                temp_message.sender_ip_address = _ip_address.ipv4_numeric();
                temp_message.target_ethernet_address = message.sender_ethernet_address;
                temp_message.target_ip_address = message.sender_ip_address;
                temp_message.opcode = ARPMessage::OPCODE_REPLY;
                temp_frame.payload() = temp_message.serialize();

                // 入队
                frames_out().push(temp_frame);
                // 同时，这里对方发来的ip和mac的映射也是需要存到map里面的
                ARPMapPayload payload;
                payload.ethernet_address_ = message.sender_ethernet_address;
                payload.ttl_ = MAP_TIME_;
                ip_mac_map_[message.sender_ip_address] = payload;

            } else if (message.opcode == ARPMessage::OPCODE_REPLY) {
                // 二次判断：发错人了，丢弃
                if (message.target_ip_address != _ip_address.ipv4_numeric()) {
                    return {};
                }
                // 收到了之前广播的回复，填充MAC地址
                ARPMapPayload payload;
                payload.ethernet_address_ = message.sender_ethernet_address;
                payload.ttl_ = MAP_TIME_;
                ip_mac_map_[message.sender_ip_address] = payload;

                EthernetFrame temp_frame;
                temp_frame.header().src = _ethernet_address;
                temp_frame.header().dst = message.sender_ethernet_address;
                temp_frame.header().type = EthernetHeader::TYPE_IPv4;
                temp_frame.payload() = send_queue_.front().serialize();
                send_queue_.pop();
                frames_out().push(temp_frame);
            }
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // 调用tick时，map表所有ttl都要减去相应时间
    for (auto iter = ip_mac_map_.begin(); iter != ip_mac_map_.end(); iter++) {
        if (iter->second.ttl_ >= ms_since_last_tick) {
            iter->second.ttl_ -= ms_since_last_tick;
        } else {
            iter->second.ttl_ = 0;
        }
    }

    auto iter = last_second_map_.begin();
    while (iter != last_second_map_.end()) {
        if (iter->second > ms_since_last_tick) {
            iter->second -= ms_since_last_tick;
            iter++;
        } else {
            iter->second = 0;
            // 冷却时间到，移除元素
            last_second_map_.erase(iter++->first);
        }
    }
}
