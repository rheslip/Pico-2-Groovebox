// Convert a set of WAV audio files to C data arrays for the Teensy3 Audio Library
// Copyright 2014, Paul Stoffregen (paul@pjrc.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// compile with:  gcc -O2 -Wall -o wav2header wav2header.c
//                i686-w64-mingw32-gcc -s -O2 -Wall wav2header.c -o wav2header.exe

// R Heslip Feb 6/2019
// modded to generate header files from 22khz 16bit PCM wav files 
// header files are formatted specifically for my Motivation Radio drum machine/sample player sketch
// R Heslip Oct 2024 - modded into a function to convert/load wav files into memory

uint8_t read_uint8();
int16_t read_int16();
uint32_t read_uint32();
File in; 
bool EOF_error;  // added for debugging data read errors

// WAV file format:
// http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
//
// pass a null buf pointer to skip loading data 
// returns data size in words or 0 if no data/error loading file

int32_t loadwav(char * path, uint8_t * buf)
{

	uint32_t header[4];
	int16_t format, channels, bits,datasize,bytespersample;
	uint32_t rate;
	uint32_t i, length, padlength=0, arraylen;
	uint32_t chunkSize;
	int32_t audio=0;
	uint32_t skip=1; // 1 is no downsampling
  unsigned int bcount, wcount;
  unsigned int total_length=0;
  int pcm_mode=1;

  EOF_error=false;

	in = SD.open(path); // open the file

	// read the WAV file's header
	for (i=0; i < 4; i++) {
		header[i] = read_uint32();
	}
  if (header[0] != 0x46464952) return 0; // check for "WAVE" header
	while (header[3] != 0x20746D66) {
		// skip past unknown sections until "fmt "
		chunkSize = read_uint32();
		for (i=0; i < chunkSize; i++) {
			read_uint8();
		}
		header[3] = read_uint32();
	}
	chunkSize = read_uint32();

	// read the audio format parameters
	format = read_int16();
	channels = read_int16();
	rate = read_uint32();
	read_uint32(); // ignore byterate
	read_int16();  // ignore blockalign
	bits = read_int16();
  datasize=bits/8;
#ifdef DEBUG
	Serial.printf("format: %d, channels: %d, rate: %d, bits %d datasize %d\n", format, channels, rate, bits,datasize);
#endif
	if (format != 1) {
#ifdef DEBUG    
	  Serial.printf("file %s is compressed, only uncompressed supported\n", path);
    return 0;
#endif
  }
//	if (rate != 22050)
//		die("sample rate %d in %s is unsupported\n"
//		  "sample rate must be 22050", rate, filename);
	if (channels != 1 && channels != 2) {
#ifdef DEBUG
    Serial.printf("file %s has %d channels, but only 1 & 2 are supported\n", path, channels);
#endif
    return 0;
  } 
	
	if (!((bits == 16) | (bits ==24))) {
#ifdef DEBUG
    Serial.printf("file %s has %d bit format, but only 16 or 24 is supported\n", path, bits);
#endif
    return 0;
  }
	if (rate == 44100) {
#ifdef DEBUG
		Serial.printf("Resampling 44khz file to 22khz\n");
		skip=2; // write every 2nd sample
#endif
	}

	// skip past any extra data on the WAVE header (hopefully it doesn't matter?)
	for (chunkSize -= 16; chunkSize > 0; chunkSize--) {
		read_uint8();
#ifdef DEBUG
	  if (EOF_error) { 
      Serial.printf("end of data reading extra %d header bytes \n",chunkSize);
      return 0;
    }
#endif 
	}

	// read the data header, skip non-audio data
	while (1) {
		header[0] = read_uint32();
#ifdef DEBUG
	  if (EOF_error) { 
      Serial.printf("end of data reading header[0]\n");
      return 0; 
    }
#endif 
		length = read_uint32();
#ifdef DEBUG
	  if (EOF_error) { 
        Serial.printf("end of data reading data length\n");
        return 0; // size of the data read
    }
#endif 

    if (length % 2) {
      ++length;
#ifdef DEBUG
      Serial.printf("Non-audio chunk %4x length %d was rounded up\n",header[0],length);
#endif 
    }


#ifdef DEBUG
    Serial.printf("Chunk %4x length %4x\n",header[0],length);
	  if (EOF_error) { 
      Serial.printf("end of data reading data length\n");
      return 0; // size of the data read
    }
#endif 
		if (header[0] == 0x61746164) break; // beginning of actual audio data
		// skip over non-audio data
		for (i=0; i < length; i++) {
			read_uint8();
#ifdef DEBUG
	  if (EOF_error) { 
      Serial.printf("end of data reading %d non-audio section\n",length);
      return 0;
    }
#endif 
		}
	}

/*
	// the length must be a multiple of the data size
	if (channels == 2) {
		if (length % 4) {
#ifdef DEBUG
      Serial.printf("file %s data length is not a multiple of 4\n", path);
#endif
      return 0;
    } 
		length = length / 4;
	}
	if (channels == 1) {
		length = length / 2;
		if (length % 1){
#ifdef DEBUG
      Serial.printf("file %s data length is not a multiple of 2\n", path);
#endif
      return 0;
    }  
	}
	*/
  bytespersample=channels * datasize;
	if (length % bytespersample) { // check that data chunk is is correct size
#ifdef DEBUG
      Serial.printf("file %s data length is not a multiple of %d\n", path,bytespersample);
#endif
    return 0;
  }   

	length = length / bytespersample;
	if (length % 1){
#ifdef DEBUG
    Serial.printf("file %s data length is not a multiple of 2\n", path);
#endif
    return 0;
  } 

	if (length > 0xFFFFFF) {
  #ifdef DEBUG
    Serial.printf("file %s data length is too long\n", path);
  #endif
    return 0; 
  }
	bcount = 0;
	
	if (pcm_mode) {
		//arraylen = ((length + padlength) * 2 + 3) / 4 + 1;
		arraylen = length/skip; // RH skip=2 for downsampling from 44 to 22khz
		format |= 0x80;
	} else {
		arraylen = (length + padlength + 3) / 4 + 1;
	}
	total_length += arraylen;

	// output a minimal header, just the length, #bits and sample rate
	//fprintf(outh, "extern const unsigned int AudioSample%s[%d];\n", samplename, arraylen);
#ifdef DEBUG
	Serial.printf("// Converted from %s, using %d Hz, %s encoding , %d bits, %d samples\n", path, rate,
	  (pcm_mode ? "16 bit PCM" : "u-law"),bits, arraylen);
#endif
	//Serial.printf("#define %s_SIZE %d\n\n", samplename, arraylen);	  
	//fprintf(out, "const uint16_t %s[%d] = {\n", samplename, arraylen);
	//Serial.printf("const int16_t %s[] = {\n", samplename);
	//fprintf(out, "0x%08X,", length | (format << 24));
	wcount = 0;

  if (buf !=0) { // if we pass a null pointer don't load data - so we can use the same function to get the data size
	// finally, read the audio data
    while (length > 0) {
      if (channels == 1) {
        if (bits==24) read_uint8(); // toss low byte
        audio = read_int16();
#ifdef DEBUG
	      if (EOF_error) { 
          Serial.printf("end of data - expecting %d more samples\n",length);
          return (arraylen-length); // size of the data read
        }
#endif       
      } else {
        if (bits==24) read_uint8(); // toss low byte
        audio = read_int16();
        if (bits==24) read_uint8(); // toss low byte
        audio += read_int16();
        audio /= 2;
#ifdef DEBUG
	      if (EOF_error) { 
          Serial.printf("end of data - expecting %d more samples\n",length);
          return (arraylen-length); // size of the data read
        }
#endif 
      }
      if (pcm_mode) {
        if ((length % skip)==0) {// downsampling if skip >1
          *buf=audio;
          ++buf;
          *buf=audio>>8;
          ++buf;
        //	print_byte(out, audio);
        //	print_byte(out, audio >> 8);
        }
      } else {
        *buf=audio;
        ++buf;
      //	print_byte(out, ulaw_encode(audio));
      }
      length--;
    }
    while (padlength > 0) {
      *buf=0;
      ++buf;
      // print_byte(out, 0);
      padlength--;
    }
    while (bcount > 0) {
      *buf=0;
      ++buf;
      // print_byte(out, 0);
    }
  }
  return arraylen;
}

uint8_t read_uint8()
{
	int c1;

	c1 = in.read();
	if (c1 == EOF) {
#ifdef DEBUG
	  Serial.printf("error, end of data while reading file\n");
#endif
    EOF_error=true;
    return 0; 
  }
	c1 &= 255;
	return c1;
}

int16_t read_int16()
{
	int c1, c2;

	c1 = in.read();
	if (c1 == EOF) {
#ifdef DEBUG
	  Serial.printf("error, end of data while reading file\n");
#endif
    EOF_error=true;
    return 0; 
  }
	c2 = in.read();
	if (c2 == EOF) {
#ifdef DEBUG
	  Serial.printf("error, end of data while reading file\n");
#endif
    EOF_error=true;
    return 0; 
  }
	c1 &= 255;
	c2 &= 255;
	return (c2 << 8) | c1;
}

uint32_t read_uint32()
{
	int c1, c2, c3, c4;

	c1 = in.read();
	if (c1 == EOF) {
#ifdef DEBUG
	  Serial.printf("error, end of data while reading file\n");
#endif
    EOF_error=true;
    return 0; 
  }
	c2 = in.read();
	if (c2 == EOF) {
#ifdef DEBUG
	  Serial.printf("error, end of data while reading file\n");
#endif
    EOF_error=true;
    return 0; 
  }
	c3 = in.read();
	if (c3 == EOF) {
#ifdef DEBUG
	  Serial.printf("error, end of data while reading file\n");
#endif
    EOF_error=true;
    return 0; 
  }
	c4 = in.read();
	if (c4 == EOF) {
#ifdef DEBUG
	  Serial.printf("error, end of data while reading file\n");
#endif
    EOF_error=true;
    return 0; 
  }
	c1 &= 255;
	c2 &= 255;
	c3 &= 255;
	c4 &= 255;
	return (c4 << 24) | (c3 << 16) | (c2 << 8) | c1;
}



