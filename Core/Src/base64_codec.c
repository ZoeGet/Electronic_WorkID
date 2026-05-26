#include "base64_codec.h"

static const char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

uint16_t Base64_EncodedLength(uint16_t inputLength)
{
    return (uint16_t)(((inputLength + 2U) / 3U) * 4U);
}

uint16_t Base64_Encode(const uint8_t *input, uint16_t inputLength, char *output, uint16_t outputSize)
{
    uint16_t inputIndex = 0U;
    uint16_t outputIndex = 0U;
    uint16_t encodedLength;

    if ((input == 0) || (output == 0))
    {
        return 0U;
    }

    encodedLength = Base64_EncodedLength(inputLength);
    if ((outputSize == 0U) || (encodedLength >= outputSize))
    {
        return 0U;
    }

    while (inputIndex < inputLength)
    {
        uint32_t octetA = input[inputIndex++];
        uint32_t octetB = (inputIndex < inputLength) ? input[inputIndex++] : 0U;
        uint32_t octetC = (inputIndex < inputLength) ? input[inputIndex++] : 0U;
        uint32_t triple = (octetA << 16) | (octetB << 8) | octetC;

        output[outputIndex++] = kBase64Alphabet[(triple >> 18) & 0x3FU];
        output[outputIndex++] = kBase64Alphabet[(triple >> 12) & 0x3FU];
        output[outputIndex++] = kBase64Alphabet[(triple >> 6) & 0x3FU];
        output[outputIndex++] = kBase64Alphabet[triple & 0x3FU];
    }

    if ((inputLength % 3U) == 1U)
    {
        output[encodedLength - 1U] = '=';
        output[encodedLength - 2U] = '=';
    }
    else if ((inputLength % 3U) == 2U)
    {
        output[encodedLength - 1U] = '=';
    }

    output[encodedLength] = '\0';
    return encodedLength;
}
