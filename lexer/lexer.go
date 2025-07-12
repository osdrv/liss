package lexer

import (
	"bytes"
	"fmt"
	"osdrv/liss/token"
)

type LexerMode uint8

const (
	LexerModeDefault LexerMode = iota
	LexerModeString
)

type Lexer struct {
	Source string
	Tokens []token.Token

	pos    int
	line   int
	column int

	lastError error
}

func NewLexer(source string) *Lexer {
	return &Lexer{
		Source: source,
		Tokens: make([]token.Token, 0),
		pos:    0,
		line:   1,
		column: 1,
	}
}

func (l *Lexer) Err() error {
	return l.lastError
}

func (l *Lexer) NextToken() token.Token {
	l.skipWhitespace()
	switch {
	case l.isEOF():
		return l.emitEOF()
	case l.isNumeric():
		return l.emitNumeric()
	case l.isString():
		return l.emitString()
	default:
		return l.emitError("Unexpected character `" + string(l.Source[l.pos]) + "`")
	}
}

func (l *Lexer) emitEOF() token.Token {
	return token.Token{
		Type: token.EOF,
		Location: token.Location{
			Line:   l.line,
			Column: l.column,
		},
	}
}

func (l *Lexer) advance(mode LexerMode) {
	l.pos++
	l.column++
	if mode != LexerModeString {
		if !l.isEOF() && isNewline(l.Source[l.pos]) {
			l.pos++
			l.line++
			l.column = 1
		}
	}
}

func (l *Lexer) emitString() token.Token {
	var b bytes.Buffer
	from := l.pos
	loc := token.Location{
		Line:   l.line,
		Column: l.column,
	}
	match := l.Source[from]
	isDoubleQuote := match == '"'
	escapeAt := -1
	for {
		l.advance(LexerModeString)
		if l.isEOF() {
			return l.emitError("Unterminated string literal")
		}
		ch := l.Source[l.pos]
		if ch == '\\' && isDoubleQuote && (escapeAt != l.pos) {
			escapeAt = l.pos + 1 // mark the position after the escape character
			continue             // skip the escape character
		}
		if match == ch && (escapeAt != l.pos) {
			l.advance(LexerModeDefault)
			break
		}
		b.WriteByte(ch)
	}
	return token.Token{
		Type:     token.String,
		Literal:  b.String(), // strip quotes
		Location: loc,
	}
}

func (l *Lexer) emitNumeric() token.Token {
	tok := token.Token{
		Type: token.Numeric,
		Location: token.Location{
			Line:   l.line,
			Column: l.column,
		},
	}
	from := l.pos
	to := from
	seenExponent := false
	seenDecimal := false
	for !l.isEOF() {
		ch := l.Source[l.pos]
		pass := false
		if isDigit(ch) {
			pass = true
		} else if isSign(ch) {
			if from == to || (l.pos > 0 && isExponent(l.Source[l.pos-1])) {
				pass = true
			}
		} else if ch == '.' {
			if !seenDecimal {
				seenDecimal = true
				pass = true
			}
		} else if isExponent(ch) {
			if to > from && !seenExponent {
				seenExponent = true
				pass = true
			}
		}
		if !pass {
			break
		}
		to++
		l.advance(LexerModeDefault)
	}
	tok.Literal = l.Source[from:to]
	return tok
}

func (l *Lexer) emitError(msg string) token.Token {
	err := fmt.Errorf("Syntax error: %s at line: %d column: %d", msg, l.line, l.column)
	l.lastError = err
	return token.Token{
		Type: token.Error,
		Location: token.Location{
			Line:   l.line,
			Column: l.column,
		},
	}
}

func (l *Lexer) isEOF() bool {
	return l.pos >= len(l.Source)
}

func (l *Lexer) isNumeric() bool {
	return l.pos < len(l.Source) && isDigit(l.Source[l.pos]) ||
		(l.pos+1 < len(l.Source) &&
			isSign(l.Source[l.pos]) && isDigit(l.Source[l.pos+1]))
}

func (l *Lexer) isString() bool {
	return !l.isEOF() && isQuote(l.Source[l.pos])
}

func (l *Lexer) skipWhitespace() {
	for !l.isEOF() {
		ch := l.Source[l.pos]
		if !isWhitespace(ch) {
			break
		}
		l.advance(LexerModeDefault)
	}
}

func isQuote(ch byte) bool {
	return ch == '"' || ch == '\''
}

func isSign(ch byte) bool {
	return ch == '+' || ch == '-'
}

func isExponent(ch byte) bool {
	return ch == 'e' || ch == 'E'
}

func isDigit(ch byte) bool {
	return ch >= '0' && ch <= '9'
}

func isWhitespace(ch byte) bool {
	return ch == ' ' || ch == '\t' || isNewline(ch)
}

func isNewline(ch byte) bool {
	return ch == '\n' || ch == '\r'
}
