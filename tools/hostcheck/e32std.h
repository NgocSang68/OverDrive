#ifndef STUB_E32STD_H
#define STUB_E32STD_H
#include <stdint.h>
#include <string.h>
typedef int TInt; typedef unsigned int TUint; typedef unsigned int TUint32; typedef unsigned char TUint8; typedef unsigned short TUint16; typedef short TInt16;
typedef long long TInt64_; typedef int TBool; typedef unsigned char TText8; typedef unsigned short TText16;
enum { EFalse = 0, ETrue = 1 };
enum { KErrNone = 0, KErrNotFound = -1, KErrGeneral = -2, KErrNotSupported = -5, KErrArgument = -6 };
class TInt64 { public: long long v; TInt64():v(0){} TInt64(long long x):v(x){} operator long long() const {return v;} TInt Int() const {return (TInt)v;} };
class TDesC;
class TDesC8 { public: virtual ~TDesC8(){} const TUint8* Ptr() const { return (const TUint8*)d; } TInt Length() const { return n; } TInt Size() const { return n; }
  const TUint8& operator[](TInt i) const { return d[i]; }
  TUint8* d; TInt n; TInt max; };
class TDes8 : public TDesC8 { public: TInt Compare(const TDesC8&) const {return 0;} void SetMax(){}  TInt MaxLength() const { return max; } void Zero(){ n=0; } void SetLength(TInt l){ n=l; }
  void Format(const TDesC8& f, ...) {} void Copy(const TDesC8&) {} void Copy(const TDesC&) {} void Copy(const TUint8*, TInt) {} void Append(const TDesC8&) {} void Append(const TUint8*, TInt){} void Append(TUint8){} TUint8* Ptr_() { return d; } TUint8& operator[](TInt i) { return d[i]; } };
typedef TDes8 TDes8_; 
class TPtrC;
class TDesC { public: virtual ~TDesC(){} TInt Length() const { return n; } const TUint16* Ptr() const { return d; } TUint16* d; TInt n; TInt max;
  TDesC Left(TInt) const { return *this; }
  const TUint16& operator[](TInt i) const { return d[i]; }
  TPtrC Mid(TInt, TInt) const; TInt Locate(TUint16) const { return 0; } };
class TPtrC : public TDesC { public: TPtrC(){ d=0; n=0; max=0; } TPtrC(const TDesC& x){ d=x.d; n=x.n; max=x.max; } TPtrC(const TUint16* p, TInt l){ d=(TUint16*)p; n=l; max=l; } void Set(const TDesC& x){ d=x.d; n=x.n; max=x.max; } };
inline TPtrC TDesC::Mid(TInt, TInt) const { return TPtrC(*this); }
class TDes : public TDesC { public: TInt MaxLength() const { return max; } void Zero(){ n=0; } void Format(const TDesC& f, ...) {} void Copy(const TDesC&) {} void Copy(const TDesC8&) {} void Append(const TDesC&) {} void Append(const TUint16*, TInt) {} void AppendFormat(const TDesC&, ...) {} };
template<int N> class TBuf8 : public TDes8 { TUint8 buf[N]; public: TBuf8(){ d=buf; n=0; max=N; } TBuf8(const TDesC8&){ d=buf; n=0; max=N; } template<int M> TBuf8& operator=(const TBuf8<M>&){return *this;} };
template<int N> class TBuf : public TDes { TUint16 buf[N]; public: TBuf(){ d=buf; n=0; max=N; } };
typedef TDes8 TPtr8_;
class TPtr8 : public TDes8 { public: TPtr8(TUint8*, TInt){} TPtr8(TUint8*, TInt, TInt){} };
class TPtrC8 : public TDesC8 { public: TPtrC8(const TUint8*, TInt){} TPtrC8(const TUint8*){} TPtrC8(){} };
#define _L8(x) (*(const TDesC8*)0)
#define _L(x) (*(const TDesC*)0)
#define _LIT8(n,s) const TDesC8& n = *(const TDesC8*)0
class TLex8 { public: TLex8(const TDesC8&){} void SkipSpace(){} TInt Val(TInt&){return 0;} TInt Val(TUint32&, int){return 0;} };
class TTimeIntervalMicroSeconds { public: TInt64 Int64() const { return TInt64(); } };
class TTimeIntervalSeconds { public: TInt Int() const { return 0; } };
class TTime { public: TTime(){} void UniversalTime(){} TTimeIntervalMicroSeconds MicroSecondsFrom(const TTime&) const { return TTimeIntervalMicroSeconds(); } TInt SecondsFrom(TTime, TTimeIntervalSeconds&) const { return 0; } };
class CBase { public: virtual ~CBase(){} };
class TRequestStatus;
class User { public: static void ResetInactivityTime(){} static TInt LoadLogicalDevice(const TDesC&){return 0;} static TInt IsRomAddress(TBool&, void*){return 0;} static TInt FreeLogicalDevice(const TDesC&){return 0;}  static void After(TInt){} static void* Alloc(TInt){return 0;} static void Free(void*){} static TUint32 NTickCount(){return 0;} static void Leave(TInt){} static void WaitForRequest(TRequestStatus&){} static void WaitForRequest(TRequestStatus&,TRequestStatus&){} static void LeaveIfError(TInt){} static void Panic(const TDesC&,TInt){} };
class Mem { public: static void Copy(void*, const void*, TInt){} static void FillZ(void*, TInt){} static void Fill(void*,TInt,TUint8){} };
class RHandleBase { public: void Close(){} };
class TEntry { public: TBool IsDir() const {return 0;} };
class RFs { public: TInt Connect(){return 0;} void Close(){} TInt MkDir(const TDesC&){return 0;} TInt Delete(const TDesC&){return 0;} TInt Entry(const TDesC&, TEntry&) const {return 0;} };
class CTrapCleanup { public: static CTrapCleanup* New(){ return 0; } };
#define KRequestPending (-0x7fffffff)
class RFile { public: TInt Open(RFs&, const TDesC&, TUint){return 0;} TInt Replace(RFs&, const TDesC&, TUint){return 0;} TInt Create(RFs&, const TDesC&, TUint){return 0;} TInt Read(TDes8&){return 0;} TInt Write(const TDesC8&){return 0;} TInt Flush(){return 0;} void Close(){} TInt Seek(int, TInt&){return 0;} };
enum { EFileRead=1, EFileWrite=2, EFileShareAny=4, EFileStream=8 };
class RLibrary {};
class RDevice {};
class RBusLogicalChannel : public RHandleBase { public: TInt DoControl(TInt){return 0;} TInt DoControl(TInt, void*){return 0;} TInt DoControl(TInt, void*, void*){return 0;} template<class A,class B,class C,class D,class E> TInt DoCreate(const TDesC&, const A&, B, C, D, E){return 0;} };
class RHeap {};
class UserHal {}; 
class HAL { public: static TInt Get(TInt, TInt&){return 0;} }; 
namespace HALData { enum { EFastCounterFrequency, ECPUSpeed, EMachineUid }; }
typedef void TAny;
#define _LIT(n,s) const TDesC& n = *(const TDesC*)0
#define IMPORT_C
#define EXPORT_C
#define NONSHARABLE_CLASS(x) class x
class CleanupStack { public: static void PopAndDestroy(TInt){} static void PopAndDestroy(){} static void PopAndDestroy(void*){} static void Pop(){} static void Pop(void*){} static void PushL(void*){} };
class TVersion { public: TVersion(){} TVersion(int,int,int){} };
class TDesC16;
#define LOCAL_C static
#define GLDEF_C

#define TRAPD(err, stmt) TInt err = 0; { stmt; }
#define TRAP_IGNORE(stmt) { stmt; }
#define KErrAlreadyExists (-11)
#define KNullUnit 0
enum TOwnerType { EOwnerProcess, EOwnerThread };
class TRequestStatus { public: TInt Int() const { return 0; } };
enum TThreadPriority { EPriorityNull=-30, EPriorityMuchLess=-20, EPriorityLess=-10, EPriorityNormal=0, EPriorityMore=10, EPriorityMuchMore=20, EPriorityRealTime=30, EPriorityAbsoluteVeryLow=100 };
typedef TInt (*TThreadFunction)(TAny*);
enum TExitType { EExitKill, EExitTerminate, EExitPanic, EExitPending };
class RTimer { public: TInt CreateLocal(){return 0;} void After(TRequestStatus&,TInt){} void Cancel(){} void Close(){} };
class RThread { public: TExitType ExitType() const { return EExitKill; } TInt ExitReason() const { return 0; } TInt Create(const TDesC&, TThreadFunction, TInt, TInt, TInt, TAny*, TOwnerType=EOwnerProcess){return 0;} void SetPriority(TThreadPriority) const {} void Resume() const {} void Logon(TRequestStatus&) const {} void Close(){} };
enum TSeek { ESeekStart, ESeekCurrent, ESeekEnd };
#define EFileStreamText 16
class TFileName : public TDes { TUint16 b[256]; public: TFileName(){ d=b; n=0; max=256; } };
template<class T> void CleanupClosePushL(T&) {}
void __dummy();
#endif
