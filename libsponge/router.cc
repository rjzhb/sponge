#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    RouterTableTuple tuple;
    tuple.route_prefix_ = route_prefix;
    tuple.prefix_length_ = prefix_length;
    tuple.next_hop_ = next_hop;
    tuple.interface_num_ = interface_num;
    router_record_.emplace_back(tuple);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    uint32_t dst_ip = dgram.header().dst;
    // route_prefix = 18.6.1.0
    // prefix_length = 31
    auto max_length_tuple = router_record_.end();
    // 遍历路由表
    for (auto iter = router_record_.begin(); iter != router_record_.end(); iter++) {
        // 通过异或运算判定路由是否匹配
        if (iter->prefix_length_ == 0 || (dst_ip ^ iter->route_prefix_) >> (32 - iter->prefix_length_) == 0) {
            // 找到最大前缀匹配
            if (max_length_tuple == router_record_.end() || max_length_tuple->prefix_length_ < iter->prefix_length_) {
                max_length_tuple = iter;
            }
        }
    }

    // 判断报文是否过期,ttl--，没过期就封装成报文发送
    if (max_length_tuple != router_record_.end() && dgram.header().ttl-- > 1) {
        // 路由表是否有下一跳
        if (max_length_tuple->next_hop_.has_value()) {
            _interfaces[max_length_tuple->interface_num_].send_datagram(dgram, max_length_tuple->next_hop_.value());
        } else {
            // 路由表没有下一跳，直接发送数据包
            _interfaces[max_length_tuple->interface_num_].send_datagram(dgram, Address::from_ipv4_numeric(dst_ip));
        }
    }

    // 其他情况丢弃数据包
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
