#ifndef MJ_YAML_H
#define MJ_YAML_H

#include <stdint.h>
#include <stdio.h>
#include <variant>

namespace mj
{
struct YamlParser;

enum class EYamlEventType
{
  None,
  StreamStart,
  StreamEnd,
  DocumentStart,
  DocumentEnd,
  Alias,
  Scalar,
  SequenceStart,
  SequenceEnd,
  MappingStart,
  MappingEnd
};

enum class EYamlScalarStyle
{
  Any,
  Plain,
  SingleQuoted,
  DoubleQuoted,
  Literal,
  Folded
};

enum class EYamlSequenceStyle
{
  Any,
  Block,
  Flow
};

enum class EYamlMappingStyle
{
  Any,
  Block,
  Flow,
};

enum class EYamlEncoding
{
  Any,
  Utf8,
  Utf16Le,
  Utf16Be
};

struct YamlVersionDirective
{
  int32_t major = 0;
  int32_t minor = 0;
};

struct YamlTagDirective
{
  uint8_t* handle = nullptr;
  uint8_t* prefix = nullptr;
};

enum class EYamlError
{
  None,
  Memory,
  Reader,
  Scanner,
  Parser,
  Composer,
  Writer,
  Emitter
};

struct YamlMark
{
  size_t index  = 0;
  size_t line   = 0;
  size_t column = 0;
};

struct YamlEvent
{
  EYamlEventType type = EYamlEventType::None;

  struct stream_start_t
  {
    EYamlEncoding encoding = EYamlEncoding::Any;
  };

  struct tag_directives_t
  {
    YamlTagDirective* start = nullptr;
    YamlTagDirective* end   = nullptr;
  };

  struct document_start_t
  {
    YamlVersionDirective* version_directive = nullptr;

    tag_directives_t tag_directives;

    bool implicit = false;
  };

  struct document_end_t
  {
    bool implicit = false;
  };

  struct alias_t
  {
    uint8_t* anchor = nullptr;
  };

  struct scalar_t
  {
    uint8_t* anchor        = nullptr;
    uint8_t* tag           = nullptr;
    uint8_t* value         = nullptr;
    size_t length          = 0;
    bool plain_implicit    = false;
    bool quoted_implicit   = false;
    EYamlScalarStyle style = EYamlScalarStyle::Any;
  };

  struct sequence_start_t
  {
    uint8_t* anchor          = nullptr;
    uint8_t* tag             = nullptr;
    int implicit             = 0;
    EYamlSequenceStyle style = EYamlSequenceStyle::Any;
  };

  struct mapping_start_t
  {
    uint8_t* anchor         = nullptr;
    uint8_t* tag            = nullptr;
    int implicit            = 0;
    EYamlMappingStyle style = EYamlMappingStyle::Any;
  };

  std::variant<stream_start_t, document_start_t, document_end_t, alias_t, scalar_t,
               sequence_start_t, mapping_start_t>
      data;

  YamlMark start_mark;
  YamlMark end_mark;

public:
  void Init(EYamlEventType type, const YamlMark& start_mark, const YamlMark& end_mark);
  void InitStreamStart(EYamlEncoding encoding, const YamlMark& start_mark,
                       const YamlMark& end_mark);
  void InitStreamEnd(const YamlMark& start_mark, const YamlMark& end_mark);
  void InitDocumentStart(YamlVersionDirective version_directive,
                         YamlTagDirective* tag_directives_start,
                         YamlTagDirective* tag_directives_end, bool implicit,
                         const YamlMark& start_mark, const YamlMark& end_mark);
  void InitDocumentEnd(bool implicit, const YamlMark& start_mark, const YamlMark& end_mark);
  void InitAlias(uint8_t* anchor, const YamlMark& start_mark, const YamlMark& end_mark);
  void InitScalar(uint8_t* anchor, uint8_t* tag, uint8_t* value, size_t length, bool plain_implicit,
                  bool quoted_implicit, EYamlScalarStyle style, const YamlMark& start_mark,
                  const YamlMark& end_mark);
  void InitSequenceStart(uint8_t* anchor, uint8_t* tag, bool implicit, EYamlSequenceStyle style,
                         const YamlMark& start_mark, const YamlMark& end_mark);
  void InitSequenceEnd(const YamlMark& start_mark, const YamlMark& end_mark);
  void InitMappingStart(uint8_t* anchor, uint8_t* tag, bool implicit, EYamlMappingStyle style,
                        const YamlMark& start_mark, const YamlMark& end_mark);
  void InitMappingEnd(const YamlMark& start_mark, const YamlMark& end_mark);

  void Delete(YamlParser& parser);
}; // YamlEvent

enum class EYamlTokenType
{
  None,
  StreamStart,
  StreamEnd,
  VersionDirective,
  TagDirective,
  DocumentStart,
  DocumentEnd,
  BlockSequenceStart,
  BlockMappingStart,
  BlockEnd,
  FlowSequenceStart,
  FlowSequenceEnd,
  FlowMappingStart,
  FlowMappingEnd,
  BlockEntry,
  FlowEntry,
  Key,
  Value,
  Alias,
  Anchor,
  Tag,
  Scalar
};

struct YamlToken
{
  struct stream_start_t
  {
    EYamlEncoding encoding = EYamlEncoding::Any;
  };

  struct alias_t
  {
    uint8_t* value = nullptr;
  };

  struct anchor_t
  {
    uint8_t* value = nullptr;
  };

  struct tag_t
  {
    uint8_t* handle = nullptr;
    uint8_t* suffix = nullptr;
  };

  struct scalar_t
  {
    uint8_t* value         = nullptr;
    size_t length          = 0;
    EYamlScalarStyle style = EYamlScalarStyle::Any;
  };

  struct version_directive_t
  {
    int major = 0;
    int minor = 0;
  };

  struct tag_directive_t
  {
    uint8_t* handle = nullptr;
    uint8_t* prefix = nullptr;
  };

  EYamlTokenType type = EYamlTokenType::None;

  std::variant<stream_start_t, alias_t, anchor_t, tag_t, scalar_t, version_directive_t,
               tag_directive_t>
      data;

  YamlMark start_mark;
  YamlMark end_mark;

  static YamlToken Init(EYamlTokenType token_type, const YamlMark& token_start_mark,
                        const YamlMark& token_end_mark);
  static YamlToken InitStreamStart(EYamlEncoding token_encoding, const YamlMark& start_mark,
                                   const YamlMark& end_mark);
  static YamlToken InitStreamEnd(const YamlMark& start_mark, const YamlMark& end_mark);
  static YamlToken InitAlias(uint8_t* token_value, const YamlMark& start_mark,
                             const YamlMark& end_mark);
  static YamlToken InitAnchor(uint8_t* token_value, const YamlMark& start_mark,
                              const YamlMark& end_mark);
  static YamlToken InitTag(uint8_t* token_handle, uint8_t* token_suffix, const YamlMark& start_mark,
                           const YamlMark& end_mark);
  static YamlToken InitScalar(uint8_t* token_value, size_t token_length,
                              EYamlScalarStyle token_style, const YamlMark& start_mark,
                              const YamlMark& end_mark);
  static YamlToken InitVersionDirective(int token_major, int token_minor,
                                        const YamlMark& start_mark, const YamlMark& end_mark);
  static YamlToken InitTagDirective(uint8_t* token_handle, uint8_t* token_prefix,
                                    const YamlMark& start_mark, const YamlMark& end_mark);

  void Delete(YamlParser& parser);
};

enum class EYamlNodeType
{
  None,
  Scalar,
  Sequence,
  Mapping
};

struct YamlNodePair
{
  int key   = 0;
  int value = 0;
};

template <typename T>
struct YamlStack
{
  T* start = nullptr;
  T* end   = nullptr;
  T* top   = nullptr;

  bool Init(YamlParser& parser);
  void Del(YamlParser& parser);
  bool Empty();
  bool Limit(YamlParser& parser, size_t size);
  bool Push(YamlParser& parser, const T& value);
  const T& Pop();
  bool Extend(YamlParser& parser);
};

struct YamlNode
{
  EYamlNodeType type;

  uint8_t* tag;

  struct scalar_t
  {
    uint8_t* value         = nullptr;
    size_t length          = 0;
    EYamlScalarStyle style = EYamlScalarStyle::Any;
  };

  struct sequence_t
  {
    YamlStack<YamlNode> items;
    EYamlSequenceStyle style = EYamlSequenceStyle::Any;
  };

  struct mapping_t
  {
    YamlStack<YamlNodePair> pairs;
    EYamlMappingStyle style = EYamlMappingStyle::Any;
  };

  std::variant<scalar_t, sequence_t, mapping_t> data;

  YamlMark start_mark;
  YamlMark end_mark;
};

struct YamlDocument
{
  YamlStack<YamlNode> nodes;

  YamlVersionDirective* version_directive = nullptr;

  struct
  {
    YamlTagDirective* start = nullptr;
    YamlTagDirective* end   = nullptr;
  } tag_directives;

  bool start_implicit = false;
  bool end_implicit   = false;

  YamlMark start_mark;
  YamlMark end_mark;
};

typedef int yaml_read_handler_t(YamlParser& parser, unsigned char* buffer, size_t size, size_t* size_read);

struct YamlSimpleKey
{
  bool possible       = false;
  bool required       = false;
  size_t token_number = 0;
  YamlMark mark;
};

enum class EYamlParserState
{
  StreamStart,
  ImplicitDocumentStart,
  DocumentStart,
  DocumentContent,
  DocumentEnd,
  BlockNode,
  BlockNodeOrIndentlessSequence,
  FlowNode,
  BlockSequenceFirstEntry,
  BlockSequenceEntry,
  IndentlessSequenceEntry,
  BlockMappingFirstKey,
  BlockMappingKey,
  BlockMappingValue,
  FlowSequenceFirstEntry,
  FlowSequenceEntry,
  FlowSequenceEntryMappingKey,
  FlowSequenceEntryMappingValue,
  FlowSequenceEntryMappingEnd,
  FlowMappingFirstKey,
  FlowMappingKey,
  FlowMappingValue,
  FlowMappingEmptyValue,
  End
};

struct YamlString
{
  uint8_t* start   = nullptr;
  uint8_t* end     = nullptr;
  uint8_t* pointer = nullptr;

  bool Init(YamlParser& parser, size_t size);
  bool CheckAt(char octet, size_t offset = 0);
  bool IsAlphaAt(size_t offset = 0);
  bool IsDigitAt(size_t offset = 0);
  uint8_t AsDigitAt(size_t offset = 0);
  bool IsHexAt(size_t offset = 0);
  uint8_t AsHexAt(size_t offset = 0);
  bool IsAsciiAt(size_t offset = 0);
  bool IsPrintableAt(size_t offset = 0);
  bool IsNulAt(size_t offset = 0);
  bool IsBomAt(size_t offset = 0);
  bool IsSpaceAt(size_t offset = 0);
  bool IsTabAt(size_t offset = 0);
  bool IsBlankAt(size_t offset = 0);
  bool IsBreakAt(size_t offset = 0);
  bool IsCrlfAt(size_t offset = 0);
  bool IsBreakOrNulAt(size_t offset = 0);
  bool IsSpaceOrNulAt(size_t offset = 0);
  bool IsBlankOrNulAt(size_t offset = 0);
  size_t WidthAt(size_t offset = 0);

  bool MOVE();
  bool COPY(YamlString& string_b);

  // former macros
  bool Extend(YamlParser& parser);
  void Clear();
  bool Join(YamlParser& parser, YamlString& string_b);
  void Del(YamlParser& parser);
};

struct YamlBuffer : public YamlString
{
  uint8_t* last = nullptr;

  bool Init(YamlParser& parser, size_t size);
  void Del(YamlParser& parser);
};

struct YamlRawBuffer : public YamlBuffer
{
  //unsigned char* last    = nullptr;
};

struct YamlAlias
{
  uint8_t* anchor = nullptr;
  int index       = 0;
  YamlMark mark;
};

typedef void* (*YamlMallocFn)(size_t size);
typedef void* (*YamlReallocFn)(void* ptr, size_t size);
typedef void (*YamlFreeFn)(void* ptr);
typedef char* (*YamlStrdupFn)(const char*);

template <typename T>
struct YamlQueue
{
  T* start = nullptr;
  T* end   = nullptr;
  T* head  = nullptr;
  T* tail  = nullptr;

  bool Init(YamlParser& parser, size_t size);
  void Del(YamlParser& parser);
  bool Empty();
  bool Enqueue(YamlParser& parser, T value);
  T Dequeue();
  bool Insert(YamlParser& parser, size_t index, T value);
  bool Extend(YamlParser& parser);
};

struct YamlFns
{
  YamlMallocFn Malloc = nullptr;
  YamlReallocFn Realloc = nullptr;
  YamlFreeFn Free = nullptr;
  YamlStrdupFn Strdup = nullptr;
};

struct YamlParser
{
  YamlMallocFn Malloc   = nullptr;
  YamlReallocFn Realloc = nullptr;
  YamlFreeFn Free       = nullptr;
  YamlStrdupFn Strdup   = nullptr;

  YamlParser(const YamlFns& Fns, const unsigned char *input, size_t size);

  bool StateMachine(YamlEvent& parserEvent);
  EYamlError error = EYamlError::None;
  bool ExtendString(YamlString& string);
  bool JoinString(YamlString& a, YamlString& b);
  bool Parse(YamlEvent& event);

  struct string_t
  {
    const unsigned char* start   = nullptr;
    const unsigned char* end     = nullptr;
    const unsigned char* current = nullptr;
  };

  string_t input;

  //std::variant<string_t, FILE*> input;

private:
  void SkipToken();
  YamlToken* PeekToken();

  // Parser
  bool ParseStreamStart(YamlEvent& event);
  bool ParseDocumentStart(YamlEvent& event, bool isImplicit);
  bool ParseDocumentContent(YamlEvent& event);
  bool ParseDocumentEnd(YamlEvent& event);
  bool ParseNode(YamlEvent& event, bool isBlock, bool isIndentless);
  bool ParseBlockSequenceEntry(YamlEvent& event, bool isFirst);
  bool ParseIndentlessSequenceEntry(YamlEvent& event);
  bool ParseBlockMappingKey(YamlEvent& event, bool isFirst);
  bool ParseBlockMappingValue(YamlEvent& event);
  bool ParseFlowSequenceEntry(YamlEvent& event, bool isFirst);
  bool ParseFlowSequenceEntryMappingKey(YamlEvent& event);
  bool ParseFlowSequenceEntryMappingValue(YamlEvent& event);
  bool ParseFlowSequenceEntryMappingEnd(YamlEvent& event);
  bool ParseFlowMappingKey(YamlEvent& event, bool isFirst);
  bool ParseFlowMappingValue(YamlEvent& event, bool isEmpty);

  // Reader
  bool SetReaderError(const char* problem, size_t offset, int value);
  bool DetermineEncoding();
  bool UpdateRawBuffer();
  bool UpdateBuffer(size_t length);

  // Scanner
  bool SetScannerError(const char* context, YamlMark context_mark, const char* problem);
  bool FetchMoreTokens();
  bool FetchNextToken();
  bool Cache(size_t length);
  void Skip();
  void SkipLine();
  bool Read(YamlString& string);
  bool ReadLine(YamlString& string);

  bool StaleSimpleKeys();
  bool SaveSimpleKey();
  bool RemoveSimpleKey();
  bool IncreaseFlowLevel();
  bool DecreaseFlowLevel();
  bool RollIndent(ptrdiff_t column, ptrdiff_t number, EYamlTokenType type, YamlMark mark);
  bool UnrollIndent(ptrdiff_t column);

  bool FetchStreamStart();
  bool FetchStreamEnd();
  bool FetchDirective();
  bool FetchDocumentIndicator(EYamlTokenType type);
  bool FetchFlowCollectionStart(EYamlTokenType type);
  bool FetchFlowCollectionEnd(EYamlTokenType type);
  bool FetchFlowEntry();
  bool FetchBlockEntry();
  bool FetchKey();
  bool FetchValue();
  bool FetchAnchor(EYamlTokenType type);
  bool FetchTag();
  bool FetchBlockScalar(bool literal);
  bool FetchFlowScalar(bool single);
  bool FetchPlainScalar();

  bool ScanToNextToken();
  bool ScanDirective(YamlToken& token);
  bool ScanDirectiveName(YamlMark start_mark, uint8_t** name);
  bool ScanVersionDirectiveValue(YamlMark start_mark, int* major, int* minor);
  bool ScanVersionDirectiveNumber(YamlMark start_mark, int* number);
  bool ScanTagDirectiveValue(YamlMark start_mark, uint8_t** handle, uint8_t** prefix);
  bool ScanAnchor(YamlToken& token, EYamlTokenType type);
  bool ScanTag(YamlToken& token);
  bool ScanTagHandle(bool directive, YamlMark start_mark, uint8_t** handle);
  bool ScanTagUri(bool directive, uint8_t* head, YamlMark start_mark, uint8_t** uri);
  bool ScanUriEscapes(bool directive, YamlMark start_mark, YamlString* string);
  bool ScanBlockScalar(YamlToken& token, bool literal);
  bool ScanBlockScalarBreaks(int* indent, YamlString* breaks, YamlMark start_mark,
                             YamlMark* end_mark);
  bool ScanFlowScalar(YamlToken& token, bool single);
  bool ScanPlainScalar(YamlToken& token);

  bool Scan(YamlToken& token);

  bool ProcessEmptyScalar(YamlEvent& event, YamlMark mark);
  bool ProcessDirectives(YamlVersionDirective** version_directive_ref,
                         YamlTagDirective** tag_directives_start_ref,
                         YamlTagDirective** tag_directives_end_ref);
  bool AppendTagDirective(YamlTagDirective value, bool allow_duplicates, YamlMark mark);

  bool SetParserError(const char* problem, YamlMark problem_mark);
  bool SetParserErrorContext(const char* context, YamlMark context_mark, const char* problem,
                             YamlMark problem_mark);

  const char* problem   = nullptr;
  size_t problem_offset = 0;
  int problem_value     = 0;
  YamlMark problem_mark;
  const char* context = nullptr;
  YamlMark context_mark;

  yaml_read_handler_t* read_handler = nullptr;
  void* read_handler_data           = nullptr;

  bool eof = false;
  YamlBuffer buffer;
  size_t unread = 0;
  YamlRawBuffer raw_buffer;
  EYamlEncoding encoding = EYamlEncoding::Any;
  size_t offset          = 0;
  YamlMark mark;
  bool stream_start_produced = false;
  bool stream_end_produced   = false;
  int flow_level             = 0;

  YamlQueue<YamlToken> tokens;

  size_t tokens_parsed = 0;
  bool token_available = false;

  YamlStack<int> indents;

  int indent              = 0;
  bool simple_key_allowed = false;

  YamlStack<YamlSimpleKey> simple_keys;
  YamlStack<EYamlParserState> states;

  EYamlParserState state = EYamlParserState::StreamStart;

  YamlStack<YamlMark> marks;
  YamlStack<YamlTagDirective> tag_directives;
  YamlStack<YamlAlias> aliases;

  YamlDocument* document = nullptr;
};

} // namespace mj

#endif // MJ_YAML_H
