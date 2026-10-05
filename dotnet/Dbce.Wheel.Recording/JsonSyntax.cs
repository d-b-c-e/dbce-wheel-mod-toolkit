using System.Runtime.Serialization;

namespace Dbce.Wheel.Recording
{
    // DataContractJsonSerializer ignores trailing JSON and tolerates non-JSON numbers.
    // Validate the complete bounded line before asking it to populate the DTOs.
    internal sealed class JsonSyntax
    {
        private readonly string text;
        private int offset;
        private JsonSyntax(string text) { this.text = text; }
        internal static void Validate(string text)
        {
            var parser = new JsonSyntax(text);
            parser.Value(0); parser.Space();
            if (parser.offset != text.Length) throw Invalid();
        }

        private void Space()
        {
            while (offset < text.Length && (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\r' || text[offset] == '\n')) offset++;
        }
        private bool Take(char token)
        {
            Space();
            if (offset < text.Length && text[offset] == token) { offset++; return true; }
            return false;
        }
        private void Need(char token) { if (!Take(token)) throw Invalid(); }
        private void Value(int depth)
        {
            if (depth > 64) throw Invalid();
            Space();
            if (offset >= text.Length) throw Invalid();
            switch (text[offset])
            {
                case '{':
                    offset++;
                    if (Take('}')) return;
                    do { Space(); String(); Need(':'); Value(depth + 1); } while (Take(','));
                    Need('}'); return;
                case '[':
                    offset++;
                    if (Take(']')) return;
                    do { Value(depth + 1); } while (Take(','));
                    Need(']'); return;
                case '"': String(); return;
                case 't': Literal("true"); return;
                case 'f': Literal("false"); return;
                case 'n': Literal("null"); return;
                default: Number(); return;
            }
        }
        private void Literal(string value)
        {
            foreach (char character in value)
                if (offset >= text.Length || text[offset++] != character) throw Invalid();
        }
        private void String()
        {
            if (offset >= text.Length || text[offset++] != '"') throw Invalid();
            while (offset < text.Length)
            {
                char value = text[offset++];
                if (value == '"') return;
                if (value < 0x20) throw Invalid();
                if (value != '\\') continue;
                if (offset >= text.Length) throw Invalid();
                value = text[offset++];
                if (value == 'u')
                {
                    for (int i = 0; i < 4; i++)
                    {
                        if (offset >= text.Length) throw Invalid();
                        char digit = text[offset++];
                        if (!Digit(digit) && !(digit >= 'a' && digit <= 'f') && !(digit >= 'A' && digit <= 'F')) throw Invalid();
                    }
                }
                else if (value != '"' && value != '\\' && value != '/' && value != 'b' && value != 'f' && value != 'n' && value != 'r' && value != 't') throw Invalid();
            }
            throw Invalid();
        }
        private void Number()
        {
            if (offset < text.Length && text[offset] == '-') offset++;
            if (offset >= text.Length || !Digit(text[offset])) throw Invalid();
            if (text[offset] == '0') offset++;
            else Digits();
            if (offset < text.Length && text[offset] == '.') { offset++; Digits(); }
            if (offset < text.Length && (text[offset] == 'e' || text[offset] == 'E'))
            {
                offset++;
                if (offset < text.Length && (text[offset] == '+' || text[offset] == '-')) offset++;
                Digits();
            }
        }
        private void Digits()
        {
            int start = offset;
            while (offset < text.Length && Digit(text[offset])) offset++;
            if (offset == start) throw Invalid();
        }
        private static bool Digit(char value) => value >= '0' && value <= '9';
        private static SerializationException Invalid() => new SerializationException("Expected one complete JSON value with valid syntax and at most 64 nesting levels.");
    }
}
