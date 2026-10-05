using System;
using System.IO;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Xml;

namespace Dbce.Wheel.Recording
{
    internal static class JsonLines
    {
        internal const int MaxLineBytes = 1024 * 1024;
        internal const int FooterReserveBytes = 8192;
        private static readonly UTF8Encoding StrictUtf8 = new UTF8Encoding(false, true);

        private static DataContractJsonSerializer Serializer() => new DataContractJsonSerializer(typeof(SessionRecord),
            new DataContractJsonSerializerSettings
            {
                UseSimpleDictionaryFormat = true,
                DateTimeFormat = new DateTimeFormat("yyyy-MM-dd'T'HH:mm:ss.fffffff'Z'", System.Globalization.CultureInfo.InvariantCulture)
                { DateTimeStyles = System.Globalization.DateTimeStyles.AssumeUniversal | System.Globalization.DateTimeStyles.AdjustToUniversal }
            });

        internal static byte[] Encode(SessionRecord record)
        {
            using (var memory = new MemoryStream())
            {
                Serializer().WriteObject(memory, record);
                return memory.ToArray();
            }
        }

        internal static SessionRecord Decode(byte[] bytes)
        {
            // Reject ill-formed UTF-8 instead of silently replacing bytes in channel names.
            JsonSyntax.Validate(StrictUtf8.GetString(bytes));
            using (var reader = JsonReaderWriterFactory.CreateJsonReader(bytes, XmlDictionaryReaderQuotas.Max))
            {
                var record = (SessionRecord)Serializer().ReadObject(reader);
                if (reader.MoveToContent() != XmlNodeType.None) throw new SerializationException("Trailing JSON content.");
                return record;
            }
        }
    }
}
