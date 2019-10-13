#include "mj/yaml_parser.hpp"

using namespace mj;

// YamlEvent

void YamlEvent::Init(EYamlEventType type, const YamlMark& start_mark, const YamlMark& end_mark)
{
  *this            = {};
  this->type       = type;
  this->start_mark = start_mark;
  this->end_mark   = end_mark;
}

void YamlEvent::InitStreamStart(EYamlEncoding encoding, const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::StreamStart, start_mark, end_mark);
  this->data.stream_start.encoding = encoding;
}

void YamlEvent::InitStreamEnd(const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::StreamEnd, start_mark, end_mark);
}

void YamlEvent::InitDocumentStart(YamlVersionDirective version_directive, YamlTagDirective* tag_directives_start, YamlTagDirective* tag_directives_end, bool implicit, const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::DocumentStart, start_mark, end_mark);

  //this->data.document_start.version_directive    = version_directive;
  this->data.document_start.tag_directives.start = tag_directives_start;
  this->data.document_start.tag_directives.end   = tag_directives_end;
  this->data.document_start.implicit             = implicit;
}

void YamlEvent::InitDocumentEnd(bool implicit, const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::DocumentEnd, start_mark, end_mark);
  this->data.document_end.implicit = implicit;
}

void YamlEvent::InitAlias(uint8_t* anchor, const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::Alias, start_mark, end_mark);
  this->data.alias.anchor = anchor;
}

void YamlEvent::InitScalar(uint8_t* anchor, uint8_t* tag, uint8_t* value, size_t length, bool plain_implicit, bool quoted_implicit, EYamlScalarStyle style, const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::Scalar, start_mark, end_mark);
  this->data.scalar.anchor          = anchor;
  this->data.scalar.tag             = tag;
  this->data.scalar.value           = value;
  this->data.scalar.length          = length;
  this->data.scalar.plain_implicit  = plain_implicit;
  this->data.scalar.quoted_implicit = quoted_implicit;
  this->data.scalar.style           = style;
}

void YamlEvent::InitSequenceStart(uint8_t* anchor, uint8_t* tag, bool implicit, EYamlSequenceStyle style, const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::SequenceStart, start_mark, end_mark);
  this->data.sequence_start.anchor   = anchor;
  this->data.sequence_start.tag      = tag;
  this->data.sequence_start.implicit = implicit;
  this->data.sequence_start.style    = style;
}

void YamlEvent::InitSequenceEnd(const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::SequenceEnd, start_mark, end_mark);
}

void YamlEvent::InitMappingStart(uint8_t* anchor, uint8_t* tag, bool implicit, EYamlMappingStyle style, const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::MappingStart, start_mark, end_mark);
  this->data.mapping_start.anchor   = anchor;
  this->data.mapping_start.tag      = tag;
  this->data.mapping_start.implicit = implicit;
  this->data.mapping_start.style    = style;
}

void YamlEvent::InitMappingEnd(const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::MappingEnd, start_mark, end_mark);
}

// YamlParser

bool YamlParser::FetchMoreTokens()
{
  return false;
}

YamlToken* YamlParser::PeekToken()
{
  return (this->tokenAvailable || this->FetchMoreTokens()) ? this->tokens.head : nullptr;
}

void YamlParser::SkipToken()
{
  this->tokenAvailable = false;
  this->tokensParsed++;
  this->stream_end_produced = (this->tokens.head->type == EYamlTokenType::StreamEnd);
  this->tokens.head++;
}

void EVENT_INIT(YamlEvent& event, EYamlEventType event_type, const mj::YamlMark& event_start_mark, const mj::YamlMark& event_end_mark)
{
  event            = {};
  event.type       = event_type;
  event.start_mark = event_start_mark;
  event.end_mark   = event_end_mark;
}

void STREAM_START_EVENT_INIT(YamlEvent& event, EYamlEncoding event_encoding, const mj::YamlMark& start_mark, const mj::YamlMark& end_mark)
{
  EVENT_INIT(event, EYamlEventType::StreamStart, start_mark, end_mark);
  event.data.stream_start.encoding = event_encoding;
}

bool YamlParser::ParseStreamStart(YamlEvent& event)
{
  YamlToken* token = this->PeekToken();

  if (!token) return 0;

  if (token->type != EYamlTokenType::StreamStart)
  {
    //return yaml_parser_set_parser_error(parser, "did not find expected <stream-start>", token->start_mark);
    return false;
  }

  this->state = EYamlParserState::ImplicitDocumentStart;

  event.InitStreamStart(token->data.stream_start.encoding, token->start_mark, token->start_mark);
  this->SkipToken();

  return true;
}

bool YamlParser::ParseDocumentStart(YamlEvent& event, bool isImplicit)
{
  return true;
}

bool YamlParser::ParseDocumentContent(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseDocumentEnd(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseNode(YamlEvent& event, bool isBlock, bool isIndentless)
{
  return true;
}

bool YamlParser::ParseBlockSequenceEntry(YamlEvent& event, bool isFirst)
{
  return true;
}

bool YamlParser::ParseIndentlessSequenceEntry(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseBlockMappingKey(YamlEvent& event, bool isFirst)
{
  return true;
}

bool YamlParser::ParseBlockMappingValue(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseFlowSequenceEntry(YamlEvent& event, bool isFirst)
{
  return true;
}

bool YamlParser::ParseFlowSequenceEntryMappingKey(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseFlowSequenceEntryMappingValue(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseFlowSequenceEntryMappingEnd(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseFlowMappingKey(YamlEvent& event, bool isFirst)
{
  return true;
}

bool YamlParser::ParseFlowMappingValue(YamlEvent& event, bool isEmpty)
{
  return true;
}

bool YamlParser::Parse(YamlEvent& event)
{
  switch (this->state)
  {
  case EYamlParserState::StreamStart:
    return this->ParseStreamStart(event);
  case EYamlParserState::ImplicitDocumentStart:
    return this->ParseDocumentStart(event, true);
  case EYamlParserState::DocumentStart:
    return this->ParseDocumentStart(event, false);
  case EYamlParserState::DocumentContent:
    return this->ParseDocumentContent(event);
  case EYamlParserState::DocumentEnd:
    return this->ParseDocumentEnd(event);
  case EYamlParserState::BlockNode:
    return this->ParseNode(event, true, false);
  case EYamlParserState::BlockNodeOrIndentlessSequence:
    return this->ParseNode(event, true, true);
  case EYamlParserState::FlowNode:
    return this->ParseNode(event, false, false);
  case EYamlParserState::BlockSequenceFirstEntry:
    return this->ParseBlockSequenceEntry(event, true);
  case EYamlParserState::BlockSequenceEntry:
    return this->ParseBlockSequenceEntry(event, false);
  case EYamlParserState::IndentlessSequenceEntry:
    return this->ParseIndentlessSequenceEntry(event);
  case EYamlParserState::BlockMappingFirstKey:
    return this->ParseBlockMappingKey(event, true);
  case EYamlParserState::BlockMappingKey:
    return this->ParseBlockMappingKey(event, false);
  case EYamlParserState::BlockMappingValue:
    return this->ParseBlockMappingValue(event);
  case EYamlParserState::FlowSequenceFirstEntry:
    return this->ParseFlowSequenceEntry(event, true);
  case EYamlParserState::FlowSequenceEntry:
    return this->ParseFlowSequenceEntry(event, false);
  case EYamlParserState::FlowSequenceEntryMappingKey:
    return this->ParseFlowSequenceEntryMappingKey(event);
  case EYamlParserState::FlowSequenceEntryMappingValue:
    return this->ParseFlowSequenceEntryMappingValue(event);
  case EYamlParserState::FlowSequenceEntryMappingEnd:
    return this->ParseFlowSequenceEntryMappingEnd(event);
  case EYamlParserState::FlowMappingFirstKey:
    return this->ParseFlowMappingKey(event, true);
  case EYamlParserState::FlowMappingKey:
    return this->ParseFlowMappingKey(event, false);
  case EYamlParserState::FlowMappingValue:
    return this->ParseFlowMappingValue(event, false);
  case EYamlParserState::FlowMappingEmptyValue:
    return this->ParseFlowMappingValue(event, true);
  case EYamlParserState::End:
    break;
  default:
    break;
  }

  event.type = EYamlEventType::StreamEnd;
  return true;
}