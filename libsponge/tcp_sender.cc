#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    uint64_t length = 0;
    uint16_t window_size = max(_window_size, static_cast<uint16_t>(1));
    while (_bytes_in_flight < window_size) {
        TCPSegment seg;
        // set syn if it should
        if (!_set_syn) {
            seg.header().syn = true;
            _set_syn = true;
        }
        // set seqno
        seg.header().seqno = next_seqno();
        // set payload
        auto payload_size =
            min(TCPConfig::MAX_PAYLOAD_SIZE, min(window_size - _bytes_in_flight, _stream.buffer_size()));
        auto payload = _stream.read(payload_size);
        seg.payload() = Buffer(std::move(payload));
        // set fin if it should
        if (_stream.eof() && !_set_fin && _bytes_in_flight + seg.length_in_sequence_space() < window_size) {
            seg.header().fin = true;
            _set_fin = true;
        }
        // ignore empty segment, or would trap into a infinite loop...
        if ((length = seg.length_in_sequence_space()) == 0)
            break;
        // and now free to go
        _segments_out.push(seg);
        // start the timer once being pushed into the queue
        if (!_timer.is_running()) {
            _timer.restart();
        }
        // backup
        _outstanding_seg.emplace(_next_seqno, std::move(seg));

        _next_seqno += length;
        _bytes_in_flight += length;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // sweep outstands queue based on ackno
    // turn ackno into absolute-seqno
    auto absolute_ackno = unwrap(ackno, _isn, next_seqno_absolute());
    // sanity check
    if (absolute_ackno > next_seqno_absolute()) {
        return;
    }
    int is_success = 0;
    // while (_outstanding_seg.front().first < absolute_ackno) {
    while (!_outstanding_seg.empty()) {
        auto &[absolute_seqno, seg] = _outstanding_seg.front();
        if (absolute_seqno + seg.length_in_sequence_space() - 1 < absolute_ackno) {
            is_success = 1;
            _bytes_in_flight -= seg.length_in_sequence_space();
            _outstanding_seg.pop();
        } else {
            break;
        }
    }
    // back to normal
    if (is_success) {
        _timer.set_timeout(_initial_retransmission_timeout);
        _timer.restart();
        _consecutive_retrans = 0;
    }
    // if all has been acknowledged, stop the timer
    // if (_outstanding_seg.empty()) {
    if (_bytes_in_flight == 0) {
        _timer.stop();
    }
    // update window_size
    _window_size = window_size;
    // fill the window
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);
    // if expired
    if (_timer.is_timeout() && !_outstanding_seg.empty()) {
        // retransmit
        _segments_out.push(_outstanding_seg.front().second);
        // if windowsize > 0
        if (_window_size > 0) {
            // consec_retrans++
            _consecutive_retrans++;
            // double RTO
            _timer.set_timeout(2 * _timer.get_timeout());
        }
        // and now reset and start it
        _timer.restart();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retrans; }

void TCPSender::send_empty_segment() {
    // helper function to generate a empty ACK segment
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.emplace(std::move(seg));
}
