namespace Dbce.Wheel.Telemetry
{
    /// <summary>
    /// The three "Data Out" packet layouts Forza titles emit, all understood by
    /// SimHub and most dashboards. Receivers validate by <b>exact length</b>.
    /// </summary>
    /// <remarks>
    /// <para>
    /// <see cref="Sled232"/> is the Motorsport 7 "sled" (physics only).
    /// <see cref="Fm7Dash311"/> appends the 79-byte dash block directly at 232.
    /// <see cref="Horizon324"/> inserts 12 unknown bytes at 232, then the dash
    /// at 244, then one trailing byte — 324 in total. The intuitive 323 is
    /// rejected silently: SimHub logs "started receiving unprocessed data" at
    /// packet rate and never connects (measured, milestone-wheel-tools, 2026-08-30).
    /// </para>
    /// <para>
    /// OutRun2006Tweaks-FFB emits <see cref="Fm7Dash311"/>; art-of-sim-rally,
    /// iracing-arcade-wheel and cruisn-collection emit <see cref="Horizon324"/>.
    /// The two are not interchangeable: a dash profile expects one or the other.
    /// </para>
    /// </remarks>
    public enum ForzaLayout
    {
        /// <summary>232 bytes: physics sled only.</summary>
        Sled232,
        /// <summary>311 bytes: sled + dash at offset 232 (Forza Motorsport 7 "Dash").</summary>
        Fm7Dash311,
        /// <summary>324 bytes: sled + 12-byte insert + dash at 244 + 1 trailing byte (Forza Horizon 4/5).</summary>
        Horizon324,
    }
}
