using System;
using System.IO;
using System.Linq;
using Dbce.Wheel.Ffb;
using Xunit;

namespace Dbce.Wheel.Ffb.Tests
{
    public class ForceProfileTests
    {
        private const string Sample = @"
# a comment
[base@1]
description = the base tune
model.spring.strength = 0.45
model.lateral.weight  = 0.55   ; trailing comment
shaper.slewPerSecond  = 3.6
shaper.invert         = false

[base@2]
description = lighter
inherits = base@1
model.lateral.weight  = 0.35
shaper.invert         = true
";

        [Fact]
        public void ParsesValuesAndStripsComments()
        {
            var p = ForceProfile.Parse(Sample, "base@1");
            Assert.Equal("base", p.Name);
            Assert.Equal(1, p.Version);
            Assert.Equal("base@1", p.Id);
            Assert.Equal("the base tune", p.Description);
            Assert.Equal(0.45f, p.Model.SpringStrength, 4);
            Assert.Equal(0.55f, p.Model.SteeringWeight, 4);   // trailing ';' comment removed
            Assert.Equal(3.6f, p.Shaper.SlewPerSecond, 4);
            Assert.False(p.Shaper.Invert);
        }

        [Fact]
        public void InheritsAppliesParentFirstAndChildWins()
        {
            var p = ForceProfile.Parse(Sample, "base@2");
            Assert.Equal("base@2", p.Id);
            Assert.Equal("lighter", p.Description);
            Assert.Equal(0.45f, p.Model.SpringStrength, 4);   // inherited
            Assert.Equal(0.35f, p.Model.SteeringWeight, 4);   // overridden
            Assert.True(p.Shaper.Invert);
        }

        [Fact]
        public void UnsetKeysKeepTheirCodeDefaults()
        {
            // A profile that names only one key must not zero everything else.
            var p = ForceProfile.Parse("[x@1]\nmodel.spring.strength = 0.1\n", "x@1");
            Assert.Equal(0.1f, p.Model.SpringStrength, 4);
            Assert.Equal(new ForceModelSettings().LateralForceReference, p.Model.LateralForceReference, 4);
            Assert.Equal(new ForceShaper().RampSeconds, p.Shaper.RampSeconds, 4);
        }

        [Fact]
        public void UnknownKeyIsReportedNotSwallowed()
        {
            // A typo in a tuning file that silently does nothing is the worst case.
            var p = ForceProfile.Parse("[x@1]\nmodel.spring.strenth = 0.9\n", "x@1");
            Assert.Contains("model.spring.strenth", p.UnknownKeys);
            Assert.Equal(0f, p.Model.SpringStrength, 4);
        }

        [Fact]
        public void EveryDeclaredKeyIsSettableAndReadable()
        {
            // Catches a key added to the vocabulary but forgotten in Set or Get.
            foreach (var key in typeof(ForceProfile)
                     .GetField("KeyNames", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static)
                     .GetValue(null) as string[])
            {
                string value = key == "shaper.invert" ? "true" : "0.25";
                var p = ForceProfile.Parse("[k@1]\n" + key + " = " + value + "\n", "k@1");
                Assert.True(p.UnknownKeys.Count == 0, key + " was not accepted by Set()");
                Assert.False(string.IsNullOrEmpty(p.Get(key)), key + " has no Get()");
            }
        }

        [Fact]
        public void RoundTripsThroughToIni()
        {
            var original = ForceProfile.Parse(Sample, "base@2");
            var again = ForceProfile.Parse(original.ToIni(), original.Id);
            foreach (var key in ForceProfile.ListIds(original.ToIni())) { }   // ids parse
            var keys = typeof(ForceProfile)
                .GetField("KeyNames", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static)
                .GetValue(null) as string[];
            foreach (var key in keys)
                Assert.Equal(original.Get(key), again.Get(key));
        }

        [Fact]
        public void LastDefinitionWinsSoAUserFileCanOverride()
        {
            string layered = Sample + "\n[base@1]\nmodel.spring.strength = 9.9\n";
            var p = ForceProfile.Parse(layered, "base@1");
            Assert.Equal(9.9f, p.Model.SpringStrength, 4);
        }

        [Fact]
        public void ListIdsFindsEveryProfile()
        {
            var ids = ForceProfile.ListIds(Sample);
            Assert.Equal(new[] { "base@1", "base@2" }, ids.ToArray());
        }

        [Fact]
        public void MissingProfileAndBadIdAndLoopsAllThrow()
        {
            Assert.Throws<FormatException>(() => ForceProfile.Parse(Sample, "nope@1"));
            Assert.Throws<FormatException>(() => ForceProfile.Parse("[noversion]\n", "noversion"));
            Assert.Throws<FormatException>(() =>
                ForceProfile.Parse("[a@1]\ninherits = b@1\n\n[b@1]\ninherits = a@1\n", "a@1"));
        }

        // ---- the profiles this repo actually ships -----------------------------

        private static string ShippedPath()
        {
            var dir = new DirectoryInfo(AppContext.BaseDirectory);
            while (dir != null && !File.Exists(Path.Combine(dir.FullName, "profiles", "force-profiles.ini")))
                dir = dir.Parent;
            return dir == null ? null : Path.Combine(dir.FullName, "profiles", "force-profiles.ini");
        }

        [Fact]
        public void ShippedProfilesAllParseWithNoUnknownKeys()
        {
            string path = ShippedPath();
            Assert.True(path != null, "profiles/force-profiles.ini not found from " + AppContext.BaseDirectory);
            string text = File.ReadAllText(path);
            var ids = ForceProfile.ListIds(text);
            Assert.NotEmpty(ids);
            foreach (var id in ids)
            {
                var p = ForceProfile.Parse(text, id);
                Assert.True(p.UnknownKeys.Count == 0,
                    id + " has unrecognised key(s): " + string.Join(", ", p.UnknownKeys));
                Assert.False(string.IsNullOrWhiteSpace(p.Description), id + " has no description");
            }
        }

        [Fact]
        public void OutRunProfileCarriesTheGamesShippedConstants()
        {
            // These are the values OutRun2006Tweaks-FFB shipped and drove on
            // 2026-08-11. If this fails someone edited a published version in
            // place, which is the one thing the versioning forbids.
            var p = ForceProfile.Parse(File.ReadAllText(ShippedPath()), "arcade-outrun@1");
            Assert.Equal(0.45f, p.Model.SpringStrength, 4);
            Assert.Equal(0.10f, p.Model.DamperStrength, 4);
            Assert.Equal(0.55f, p.Model.SteeringWeight, 4);
            Assert.Equal(0.6f, p.Model.GripLoss, 4);
            Assert.Equal(0.8f, p.Model.WeightTransfer, 4);
            Assert.Equal(0.3f, p.Model.ShiftPulseStrength, 4);
            Assert.Equal(1.0f, p.Shaper.SoftSaturation, 4);
        }

        [Fact]
        public void AProfileDrivesTheModel()
        {
            // End to end: a file becomes a force. Centring means the output
            // opposes the steer.
            var p = ForceProfile.Parse(File.ReadAllText(ShippedPath()), "arcade-outrun@1");
            var model = new ForceModel(p.Model);
            var inputs = new ForceInputs { Steer = 0.5f, HasSteer = true, SpeedMps = 30f };
            float f = model.Compute(inputs, 1f / 60f);
            Assert.True(f < 0f, "a right steer must produce a left (centring) force, got " + f);
        }

        private static ForceProfile PresetNamed(string id)
        {
            switch (id)
            {
                case "simlite@2":        return ForceProfile.SimLite();
                case "arcade-generic@1": return ForceProfile.Arcade();
                default: throw new ArgumentException("no code preset for " + id, "id");
            }
        }

        /// <summary>
        /// A code preset and the section it names must not drift apart. The file is
        /// the source of truth; the preset is for a consumer that has no file yet,
        /// and one that quietly disagreed with the published tune would be worse
        /// than no preset at all.
        /// </summary>
        [Theory]
        [InlineData("simlite@2")]
        [InlineData("arcade-generic@1")]
        public void CodePresetMatchesTheProfileItNames(string id)
        {
            var fromFile = ForceProfile.Parse(File.ReadAllText(ShippedPath()), id);
            var preset = PresetNamed(id);

            Assert.Equal(id, preset.Id);
            Assert.Equal(fromFile.Description, preset.Description);

            var keys = (string[])typeof(ForceProfile)
                .GetField("KeyNames", System.Reflection.BindingFlags.NonPublic |
                                      System.Reflection.BindingFlags.Static)
                .GetValue(null);
            foreach (var key in keys)
                Assert.True(fromFile.Get(key) == preset.Get(key),
                    id + ": " + key + " is " + preset.Get(key) + " in code but " +
                    fromFile.Get(key) + " in profiles/force-profiles.ini");
        }

        /// <summary>
        /// Why the whole-tune presets exist at all. A model preset paired with a
        /// default <see cref="ForceShaper"/> is not the game it names: the defaults
        /// are a reasonable arcade starting point and nobody's measured tune. That
        /// pairing is what shipped as <c>simlite@1</c>.
        /// </summary>
        [Fact]
        public void AModelPresetWithADefaultShaperIsNotTheTune()
        {
            var defaults = new ForceShaper();

            // art of rally conditions nothing; the defaults add three things.
            var sim = ForceProfile.SimLite().Shaper;
            Assert.NotEqual(defaults.Deadzone, sim.Deadzone);
            Assert.NotEqual(defaults.OutputDeadband, sim.OutputDeadband);
            Assert.NotEqual(defaults.RampSeconds, sim.RampSeconds);

            // The arcade tune wants two things the defaults leave off.
            var arcade = ForceProfile.Arcade().Shaper;
            Assert.NotEqual(defaults.SoftSaturation, arcade.SoftSaturation);
            Assert.NotEqual(defaults.SlewPerSecond, arcade.SlewPerSecond);
        }

        /// <summary>
        /// Since 0.7.0 the model returns forces past full scale, and the low-speed
        /// fade SCALES a force rather than capping one. A transient several times
        /// cornering force arrives at any speed, so at walking pace the fade alone
        /// is not what stands between a collision and the wheel.
        /// </summary>
        /// <remarks>
        /// Soft saturation is. It runs before the fade and, unlike the smoothing
        /// and the slew limiter, it is NOT bypassed for an event, so it is the term
        /// that keeps an unclamped impulse bounded. A profile carrying an impact,
        /// texture or shift term with no soft saturation has nothing between the
        /// model and the peak limit but a hard clip - which is the behaviour 0.7.0
        /// argued against. Raised by art-of-sim-rally as a design rule for anyone
        /// adding an impact term; this asserts it for the tunes actually shipped.
        /// </remarks>
        [Fact]
        public void AProfileWithTransientsCarriesSoftSaturation()
        {
            string text = File.ReadAllText(ShippedPath());
            int withTransients = 0;
            foreach (var id in ForceProfile.ListIds(text))
            {
                var p = ForceProfile.Parse(text, id);
                bool transients = p.Model.ImpactStrength > 0f
                               || p.Model.TextureStrength > 0f
                               || p.Model.ShiftPulseStrength > 0f;
                if (!transients) continue;
                withTransients++;
                Assert.True(p.Shaper.SoftSaturation > 0f,
                    id + " has a transient term but no soft saturation: at walking pace the " +
                    "fade only scales it, so a low-speed impact reaches the peak limit as a hard clip");
            }
            Assert.True(withTransients >= 2, "expected the arcade tunes to carry transients");
        }

        /// <summary>
        /// The same rule as a measurement, so the claim above is a number rather
        /// than an argument: with soft saturation on, tripling the model's output
        /// does NOT triple what the wheel feels at walking pace.
        /// </summary>
        [Fact]
        public void SoftSaturationKeepsALowSpeedTransientInHand()
        {
            // arcade-generic@1, because it is the shipped arcade tune that HAS a
            // low-speed fade. arcade-outrun@1 deliberately has none - see below.
            var p = ForceProfile.Parse(File.ReadAllText(ShippedPath()), "arcade-generic@1");
            Assert.True(p.Shaper.FadeFullKmh > p.Shaper.FadeStartKmh, "this test needs a fade band");
            const float walkingPace = 5f;      // inside the 3-12 km/h band

            // isEvent: an impact bypasses the smoothing and the slew limiter, which
            // is exactly why the remaining terms have to hold.
            float atFullScale = Fresh(p).Shape(1.0f, walkingPace, 1f / 60f, true);
            float threeTimes  = Fresh(p).Shape(3.0f, walkingPace, 1f / 60f, true);
            float ratio = threeTimes / atFullScale;
            Assert.True(ratio < 1.5f,
                "a 3x transient should not arrive as 3x force at walking pace; got " + ratio +
                " (" + atFullScale + " -> " + threeTimes + ")");

            // And the test is not passing for some other reason: with soft
            // saturation off, the fade really does pass the transient through
            // linearly, which is the case art-of-sim-rally warned about.
            var off1 = Fresh(p); off1.SoftSaturation = 0f;
            var off3 = Fresh(p); off3.SoftSaturation = 0f;
            float bare1 = off1.Shape(1.0f, walkingPace, 1f / 60f, true);
            float bare3 = off3.Shape(3.0f, walkingPace, 1f / 60f, true);
            Assert.True(bare3 / bare1 > 2.9f,
                "expected the unsaturated path to scale linearly, got " + (bare3 / bare1));

            // The peak limit is the last line and holds in every case.
            Assert.True(Math.Abs(threeTimes) <= p.Shaper.PeakLimit + 1e-6f);
        }

        /// <summary>
        /// OutRun's tune has no low-speed fade at all - deliberately, because its
        /// spring term already scales with speed - so the fade is not what bounds a
        /// transient there and the warning above does not reach it. The peak limit
        /// is the bound, and that makes soft saturation the only thing separating a
        /// crash from a hard corner: without it both arrive at exactly full scale.
        /// </summary>
        [Fact]
        public void WithNoFadeSoftSaturationIsTheOnlyThingSeparatingACrashFromACorner()
        {
            var p = ForceProfile.Parse(File.ReadAllText(ShippedPath()), "arcade-outrun@1");
            Assert.False(p.Shaper.FadeFullKmh > p.Shaper.FadeStartKmh,
                "arcade-outrun@1 gained a fade; this test and the note on the profile need revisiting");

            float corner = Fresh(p).Shape(1.0f, 60f, 1f / 60f, true);
            float crash  = Fresh(p).Shape(3.0f, 60f, 1f / 60f, true);
            Assert.True(crash - corner > 0.15f,
                "a crash should be felt as bigger than a hard corner; got " + corner + " and " + crash);

            // Turn soft saturation off and the two become the same event.
            var off1 = Fresh(p); off1.SoftSaturation = 0f;
            var off3 = Fresh(p); off3.SoftSaturation = 0f;
            float flatCorner = off1.Shape(1.0f, 60f, 1f / 60f, true);
            float flatCrash  = off3.Shape(3.0f, 60f, 1f / 60f, true);
            Assert.Equal(flatCorner, flatCrash, 5);
            Assert.Equal(p.Shaper.PeakLimit, flatCrash, 5);
        }

        /// <summary>
        /// Recorded, not fixed: raising <c>shaper.strength</c> past about 60
        /// destroys the crash-versus-corner distinction that soft saturation
        /// exists to create. Gain is applied at step 7, AFTER the soft saturation
        /// at step 3, so it re-inflates an already-compressed value into the hard
        /// clamp - the same defect 0.7.0 fixed at the model boundary, reappearing
        /// one stage later.
        /// </summary>
        /// <remarks>
        /// <para>
        /// Measured on <c>arcade-outrun@1</c>, model output 1.0 (a hard corner)
        /// against 3.0 (a crash), at 60 km/h:
        /// </para>
        /// <code>
        ///   strength 50   0.7616 vs 0.9951   difference 0.2335
        ///   strength 60   0.9139 vs 1.0000   difference 0.0861
        ///   strength 75   1.0000 vs 1.0000   difference 0.0000
        /// </code>
        /// <para>
        /// Every shipped profile sets strength 50, where gain is exactly 1.0 and
        /// the ordering cannot be observed - which is why this is a test and not a
        /// golden-file row. The conformance sequence would show a zero-line diff
        /// for a fix, exactly as it did for the 0.7.0 clamp change.
        /// </para>
        /// <para>
        /// The fix is to apply gain before the soft saturation, so strength drives
        /// INTO the knee instead of past it. Deferred deliberately: it changes feel
        /// for anyone who has already moved the dial, and that is a decision to make
        /// between test sessions rather than before one. Raised by art-of-sim-rally
        /// as an ordering caveat behind the simlite@2 equivalence.
        /// </para>
        /// </remarks>
        [Fact]
        public void RaisingStrengthPastSixtyDefeatsSoftSaturation()
        {
            var p = ForceProfile.Parse(File.ReadAllText(ShippedPath()), "arcade-outrun@1");
            Assert.True(p.Shaper.SoftSaturation > 0f, "this test is about the soft knee");

            Func<int, float, float> at = (strength, model) =>
            {
                var sh = Fresh(p);
                sh.Strength = strength;
                return sh.Shape(model, 60f, 1f / 60f, true);
            };

            // At the shipped setting the knee works: a crash outweighs a corner.
            Assert.True(at(50, 3.0f) - at(50, 1.0f) > 0.2f,
                "at strength 50 a crash must still outweigh a hard corner");

            // At 75 it is gone. If this starts failing the ordering was fixed -
            // update the note on shaper.strength in profiles/force-profiles.ini,
            // which currently warns players about exactly this.
            Assert.True(at(75, 3.0f) - at(75, 1.0f) < 0.001f,
                "strength 75 no longer flattens the distinction - was the gain moved " +
                "before the soft saturation? If so this test and the profile note are stale");

            // The bound itself always holds; it is the shape that is lost.
            Assert.True(at(100, 3.0f) <= p.Shaper.PeakLimit + 1e-6f);
        }

        /// <summary>A copy of a profile's shaper with no filter state carried over.</summary>
        private static ForceShaper Fresh(ForceProfile p) => new ForceShaper
        {
            Strength = p.Shaper.Strength, Invert = p.Shaper.Invert,
            Deadzone = p.Shaper.Deadzone,
            AttackSmoothing = p.Shaper.AttackSmoothing,
            DecaySmoothing = p.Shaper.DecaySmoothing,
            SoftSaturation = p.Shaper.SoftSaturation,
            SlewPerSecond = p.Shaper.SlewPerSecond,
            OutputDeadband = p.Shaper.OutputDeadband,
            FadeStartKmh = p.Shaper.FadeStartKmh, FadeFullKmh = p.Shaper.FadeFullKmh,
            RampSeconds = 0f,                      // the ramp is start-up, not feel
            PeakLimit = p.Shaper.PeakLimit,
        };
    }
}
