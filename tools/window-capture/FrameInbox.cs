// Owns every offered frame, including rejected and late callback frames. Only
// the consumer owns a frame after Take succeeds. No graphics dependency.
internal sealed class FrameInbox<T>(int capacity) : IDisposable where T : class, IDisposable
{
    private readonly object _gate = new();
    private readonly int _capacity = capacity > 0 ? capacity : throw new ArgumentOutOfRangeException(nameof(capacity));
    private readonly Queue<T> _frames = new();
    private readonly SemaphoreSlim _available = new(0);
    private bool _stopped;
    private string? _failure;
    private long _received, _queueFull, _afterStop, _discarded, _empty;
    internal string? Failure { get { lock (_gate) return _failure; } }
    internal void Offer(T frame)
    {
        bool keep = false;
        lock (_gate)
        {
            _received++;
            if (_stopped) _afterStop++;
            else if (_frames.Count >= _capacity) _queueFull++;
            else { _frames.Enqueue(frame); _available.Release(); keep = true; }
        }
        if (!keep) frame.Dispose();
    }
    internal void Empty() { lock (_gate) _empty++; }
    internal async Task<T?> Take(int timeoutMilliseconds)
    {
        if (!await _available.WaitAsync(timeoutMilliseconds)) return null;
        lock (_gate) return _frames.Count == 0 ? null : _frames.Dequeue();
    }
    internal void Fail(string message) { lock (_gate) _failure ??= message; Stop(); }
    internal void Stop()
    {
        lock (_gate)
        {
            _stopped = true;
            while (_frames.TryDequeue(out var frame)) { _discarded++; frame.Dispose(); }
        }
    }
    internal InboxCounts Counts { get { lock (_gate) return new(_received, _queueFull, _afterStop, _discarded, _empty); } }
    public void Dispose() { Stop(); _available.Dispose(); }
}
internal sealed record InboxCounts(long Received, long DroppedQueueFull, long RejectedAfterStop, long DiscardedOnStop, long EmptyCallbacks);
