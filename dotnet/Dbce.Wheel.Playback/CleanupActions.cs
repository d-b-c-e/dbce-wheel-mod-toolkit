using System;
using System.Collections.Generic;

namespace Dbce.Wheel.Playback
{
    /// <summary>Attempt every independent restore/close, then report all failures.</summary>
    public static class CleanupActions
    {
        public static void Run(params Action[] actions)
        {
            var errors = new List<Exception>();
            foreach (var action in actions)
            {
                try { action(); }
                catch (Exception ex) { errors.Add(ex); }
            }
            if (errors.Count != 0) throw new AggregateException("Session cleanup failed", errors);
        }
    }
}
