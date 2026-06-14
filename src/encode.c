#define Byte unsigned char

void EncodeProperties(Byte *properties)
{
  properties[0] = (Byte)((pb * 5 + lp) * 9 + lc);
  Set_UInt32_LittleEndian(properties + 1, dictSize);
}
