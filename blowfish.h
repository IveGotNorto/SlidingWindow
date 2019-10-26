/********** blowfish.h **********/

#ifndef ___BLOWFISH_H___
#define ___BLOWFISH_H___


#define NUM_SUBKEYS  18
#define NUM_S_BOXES  4
#define NUM_ENTRIES  256

#define MAX_STRING   256
#define MAX_PASSWD   56  // 448bits

#if BYTE_ORDER == BIG_ENDIAN
struct WordByte
{
	unsigned int zero:8;
	unsigned int one:8;
	unsigned int two:8;
	unsigned int three:8;
};

#elif BYTE_ORDER == LITTLE_ENDIAN
struct WordByte
{
	unsigned int three:8;
	unsigned int two:8;
	unsigned int one:8;
	unsigned int zero:8;
};
#else
	#error cannot determine endianness
#endif

union Word
{
	unsigned int word;
	WordByte byte;
};

struct DWord
{
	Word word0;
	Word word1;
};


class Blowfish
{
private:
  unsigned int PA[NUM_SUBKEYS];
  unsigned int SB[NUM_S_BOXES][NUM_ENTRIES];

  void Gen_Subkeys(char *);
  inline void BF_En(Word *,Word *);
  inline void BF_De(Word *,Word *);

public:
  Blowfish();
  ~Blowfish();

  void Reset();
  void Set_Passwd(char * = NULL);
  void Encrypt(void *,unsigned int);
  void Decrypt(void *,unsigned int);
  void EncryptInput(Blowfish bf, long *size, long tsize, char *cont);
  void DecryptInput(Blowfish bf, long *size, char *cont);

};
#endif


