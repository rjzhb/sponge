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
    if (ip_mac_map_.find(dgram.header().dst) != ip_mac_map_.end()) {
        EthernetFrame ethernet_frame;
        // 设置header
        EthernetHeader ethernet_header;
        ethernet_header.type = EthernetHeader::TYPE_IPv4;
        // 设置header源地址和目的地址
        ethernet_header.src = _ethernet_address;
        ethernet_header.dst = ip_mac_map_[dgram.header().dst].ethernet_address_;
        // 设置frame
        ethernet_frame.header() = std::move(ethernet_header);
        ethernet_frame.payload() = dgram.payload();
        // 发送
        frames_out().push(ethernet_frame);
        return;
    }
    // If the destination Ethernet address is unknown, broadcast an ARP request for the
    // next hop’s Ethernet address, and queue the IP datagram so it can be sent after
    // the ARP reply is received
    InternetDatagram datagram;
    datagram.header().src = dgram.header().src;
    datagram.header().dst = next_hop_ip;
    // 入队
    send_queue_.push(datagram);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    //! If type is IPv4, returns the datagram.
    //! If type is ARP request, learn a  mapping from the "sender" fields, and send an ARP reply.
    //! If type is ARP reply, learn a mapping from the "sender" fields.
    // 抛弃无用数据段
    if (frame.header().dst != _ethernet_address) {
        return {};
    }

    uint32_t type = frame.header().type;
    if (type == frame.header().TYPE_IPv4) {
        InternetDatagram datagram;
        ParseResult res = datagram.parse(frame.payload().buffers().front());
        if (res == ParseResult::NoError) {
            return datagram;
        }
    } else if (type == frame.header().TYPE_ARP) {
        ARPMessage message;
        ParseResult res = message.parse(frame.payload().buffers().front());
        if (frame.header().dst == ETHERNET_BROADCAST) {
            ip_mac_map_[        } else {
            if (res == ParseResult::NoError) {
                ARPMapPayload padsadsaylwsoad;
                payload.ethernet_address_ = frame.header().src;
                payload.ttl_ = 30;
                ip_mac_map_[send_queue_.front().header().dst] = payload;
            }
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }
