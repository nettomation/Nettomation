/* Copyright (c) 2018 Jiri Ocenasek, http://nettomation.com
 *
 * This file is part of Nettomation.
 *
 * Nettomation is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * For tutorials, donations and proprietary licensing options
 * please visit http://nettomation.com
 */

#include "sha256.h"
#include <vector>

#define rightRotate(value, amount) ((value>>amount) | (value<<(32 - amount)))

std::string sha256(const std::string& message)
{
  std::string ascii = message;

  uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

  uint32_t hash[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

  uint64_t asciiBitLength = 8 * (uint64_t) ascii.length();
	
  ascii += '\x80'; // Append '1' bit (plus zero padding)
  while (ascii.length() % 64 - 56)
    ascii += '\x00'; // More zero padding

  std::vector<uint32_t> words(ascii.length()/4, 0);
  for (size_t i = 0; i < ascii.length(); i++) {
    words[i>>2] |= ((uint32_t) ascii[i] & 255) << ((3-i%4)*8);
  }
  words.push_back(asciiBitLength >> 32);
  words.push_back((uint32_t) asciiBitLength);

  uint32_t w[64];

  // process each chunk
  for (size_t j = 0; j < words.size(); j+=16) // 512-bit chunks
    {
      for ( size_t i = 0; i < 16; i++ )
	w[i] = words[j+i];

      for ( size_t i = 16; i < 64; i++ )
	{
	  uint32_t s0 = rightRotate(w[i-15],7) ^ rightRotate(w[i-15],18) ^ (w[i-15] >> 3);
	  uint32_t s1 = rightRotate(w[i-2],17) ^ rightRotate(w[i-2],19) ^ (w[i-2] >> 10);
	  w[i] = w[i-16] + s0 + w[i-7] + s1;
	}

      uint32_t a = hash[0];
      uint32_t b = hash[1];
      uint32_t c = hash[2];
      uint32_t d = hash[3];
      uint32_t e = hash[4];
      uint32_t f = hash[5];
      uint32_t g = hash[6];
      uint32_t h = hash[7];

      for ( size_t i = 0; i < 64; i++ )
	{
	  uint32_t S1 = rightRotate(e,6) ^ rightRotate(e,11) ^ rightRotate(e,25);
	  uint32_t ch = (e & f) ^ ((~e) & g);
	  uint32_t temp1 = h + S1 + ch + k[i] + w[i];
	  uint32_t S0 = rightRotate(a,2) ^ rightRotate(a,13) ^ rightRotate(a,22);
	  uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
	  uint32_t temp2 = S0 + maj;
 
	  h = g;
	  g = f;
	  f = e;
	  e = d + temp1;
	  d = c;
	  c = b;
	  b = a;
	  a = temp1 + temp2;
	}

      hash[0] += a;
      hash[1] += b;
      hash[2] += c;
      hash[3] += d;
      hash[4] += e;
      hash[5] += f;
      hash[6] += g;
      hash[7] += h;
    }

  char* hexTab = "0123456789abcdef";
  std::string result = "";
  for (size_t i = 0; i < 8; i++)
    {
      result += hexTab[(hash[i] >> 28) & 15];
      result += hexTab[(hash[i] >> 24) & 15];
      result += hexTab[(hash[i] >> 20) & 15];
      result += hexTab[(hash[i] >> 16) & 15];
      result += hexTab[(hash[i] >> 12) & 15];
      result += hexTab[(hash[i] >>  8) & 15];
      result += hexTab[(hash[i] >>  4) & 15];
      result += hexTab[(hash[i] >>  0) & 15];
    }

  return result;
}
