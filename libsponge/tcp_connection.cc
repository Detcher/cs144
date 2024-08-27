#include "tcp_connection.hh"

#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_seg_rcvd; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_seg_rcvd = 0;
    auto &header = seg.header();
    if (header.rst) {
        // set inbound && outbound streams to ERROR state
        reset(false);
        return;
    }
    // receiver: [SYN, FIN, seqno, payload]
    _receiver.segment_received(seg);
    // sender: [ackno, win]
    if (header.ack) {
        _sender.ack_received(header.ackno, header.win);
    }
    // LISTEN 时收到 SYN，进入 FSM 的 SYN RECEIVED 状态
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        connect();
        return;
    }
    // 判断是否为 Passive CLOSE，并进入 FSM 的 CLOSE WAIT 状态
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED) {
        _linger_after_streams_finish = false;
    }

    // Passive CLOSE，判断是否进入 FSM 的 CLOSED 状态
    if (!_linger_after_streams_finish && TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED) {
        _is_active = false;
        return;
    }
    // you! keep-alive segment!
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
        header.seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }
    segment_loaded();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    auto wr_cnt = _sender.stream_in().write(data);
    _sender.fill_window();
    segment_loaded();
    return wr_cnt;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_seg_rcvd += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // clean
        while (!_sender.segments_out().empty()) {
            _sender.segments_out().pop();
        }
        // abort the connection
        reset(true);
        return;
    }
    // load new segments from sender because of tick
    segment_loaded();
    // Active CLOSE
    if (_linger_after_streams_finish && TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
        _time_since_last_seg_rcvd >= 10 * _cfg.rt_timeout) {
        _is_active = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    // here comes the FIN
    _sender.fill_window();
    segment_loaded();
}

void TCPConnection::reset(bool send_rst) {
    if (send_rst) {
        // send a RST segment to the peer
        TCPSegment seg;
        seg.header().rst = true;
        seg.header().seqno = _sender.next_seqno();
        _segments_out.push(seg);  // use 'push()' instead
    }
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _is_active = false;
}

void TCPConnection::segment_loaded() {
    // load all prepared segments in _sender's queue into connection's queue
    while (!_sender.segments_out().empty()) {
        auto seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        // set window_size
        seg.header().win = min(_receiver.window_size(), static_cast<size_t>(numeric_limits<uint16_t>::max()));
        // set ackno && ACK
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        _segments_out.push(seg);
    }
}

void TCPConnection::connect() {
    // here comes the SYN
    _sender.fill_window();
    segment_loaded();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            reset(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
