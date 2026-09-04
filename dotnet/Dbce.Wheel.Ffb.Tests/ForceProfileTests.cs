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
    }
}
