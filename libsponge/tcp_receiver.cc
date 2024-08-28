#include "tcp_receiver.hh"

#include "stream_reassembler.hh"
#include "wrapping_integers.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto header = seg.header();
    if (not _isn.has_value()) {
        if (not header.syn)
            return;
        _isn = header.seqno;
    }
    auto absolute_seqno = unwrap(header.seqno, _isn.value(), _reassembler.stream_out().bytes_written());
    auto stream_index = static_cast<int64_t>(absolute_seqno) - 1 + (header.syn ? 1 : 0);
    if (stream_index < 0)
        return ;
    _reassembler.push_substring(seg.payload().copy(), static_cast<uint64_t>(stream_index), header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_isn.has_value())
        return nullopt;
    auto absolute_seqno =
        _reassembler.stream_out().bytes_written() + 1 + (_reassembler.stream_out().input_ended() ? 1 : 0);
    return wrap(absolute_seqno, _isn.value());
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
