int retained = TripleChecks.Run();
if (retained != 61) throw new Exception("Qualified Truckful check count changed: " + retained);
int seams = AdapterSeamChecks.Run();
int curve = CurveChecks.Run();
Console.WriteLine($"SHARED_TRIPLES_PASS {retained} retained + {seams} adapter-seam + {curve} curve-radius checks; engine-free, no Windows/game/device/display calls");
