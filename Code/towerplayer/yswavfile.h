#ifndef YSWAVFILE_IS_INCLUDED
#define YSWAVFILE_IS_INCLUDED
/* { */

#ifndef YSRESULT_IS_DEFINED
#define YSRESULT_IS_DEFINED
/*! Enum for processing result. */
typedef enum
{
	YSERR,  /*!< There were error(s). */
	YSOK    /*!< The process was successful. */
} YSRESULT;
#endif

#ifndef YSBOOL_IS_DEFINED
#define YSBOOL_IS_DEFINED
/*! Enum for boolearn. */
typedef enum
{
	YSFALSE,     /*!< False */
	YSTRUE,      /*!< True */
	YSTFUNKNOWN  /*!< Unable to tell true or false. */
} YSBOOL;
#endif

class YsWavFile
{
private:
	YSBOOL stereo;
	int bit;
	int rate;
	unsigned int sizeInBytes;

	YSBOOL isSigned;
	unsigned char *dat;

public:
	YsWavFile();
	~YsWavFile();

	void Initialize(void);

	YSBOOL Stereo(void) const;
	int BitPerSample(void) const;
	int BytePerSample(void) const;
	int PlayBackRate(void) const;
	unsigned int SizeInByte(void) const;
	YSBOOL IsSigned(void) const;
	const unsigned char *DataPointer(void) const;


	YSRESULT LoadWav(const char fn[]);
	YSRESULT ConvertTo16Bit(void);
	YSRESULT ConvertTo8Bit(void);
	YSRESULT ConvertToStereo(void);

	YSRESULT ConvertToSigned(void);
	YSRESULT ConvertToUnsigned(void);

private:
	int GetNumChannel(void) const;
	int GetNumSample(void) const;
	int GetNumSamplePerChannel(void) const;
	size_t GetUnitSize(void) const;
	size_t GetSamplePosition(int atIndex) const;
	int GetRawSignedValue(int atIndex,int channel) const;
	void SetRawSignedValue(unsigned char *unitPtr,int channel,int rawSignedValue);
};


/* } */
#endif
