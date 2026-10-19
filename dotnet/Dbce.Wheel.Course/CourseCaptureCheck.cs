using System;
using System.Linq;
using Dbce.Wheel.Playback;
using Dbce.Wheel.Telemetry;

namespace Dbce.Wheel.Course;

/// <summary>
/// Judges a route capture with <see cref="CourseComparison"/>: its own drive against its route and the game's own
/// progress, and a recorded trajectory of the same course against the same route.
/// </summary>
public static class CourseCaptureCheck
{
    /// <summary>The capture's car positions on its route, with the game's <c>progress_distance</c> as game progress.</summary>
    public static CourseComparisonResult OwnDrive(CourseRouteFile route, CourseTrackerOptions? options = null)
    {
        if (route == null) throw new ArgumentNullException(nameof(route));
        return CourseComparison.Run(route.ToPath(), route.Progress.Select(r => r.Position).ToList(),
            route.Progress.Select(r => r.ProgressDistance).ToList(), options);
    }

    /// <summary>A recorded <c>dbce.trajectory@1</c> tape (Unity world positions) on the captured route.</summary>
    public static CourseComparisonResult Trajectory(CourseRouteFile route, TrajectoryTape tape, CourseTrackerOptions? options = null)
    {
        if (route == null) throw new ArgumentNullException(nameof(route));
        if (tape == null) throw new ArgumentNullException(nameof(tape));
        return CourseComparison.Run(route.ToPath(),
            tape.Frames.Select(f => CourseAxes.FromUnity(f.Values[0], f.Values[1], f.Values[2])).ToList(), null, options);
    }

    /// <summary>True when the tape was recorded on the same game build the route was captured from.</summary>
    public static bool SameBuild(CourseRouteFile route, TrajectoryTape tape) =>
        tape.Metadata.TryGetValue("gameSha256", out var hash) &&
        string.Equals(hash, route.GameAssemblySha256, StringComparison.OrdinalIgnoreCase);
}
