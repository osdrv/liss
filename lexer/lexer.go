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
	case l.isOperator():
		return l.emitOperator()
	case l.isParenthesis():
		return l.emitParenthesis()
	case l.isKWOrIdentifier():
		return l.emitKWOrIdentifier()
	case l.isAccessor():
		return l.emitAccessor()
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
		if escapeAt == l.pos {
			switch ch {
			case 'n':
				b.WriteByte('\n')
			case 't':
				b.WriteByte('\t')
			case 'r':
				b.WriteByte('\r')
			case 'b':
				b.WriteByte('\b')
			case 'f':
				b.WriteByte('\f')
			case 'v':
				b.WriteByte('\v')
			default:
				b.WriteByte(ch)
			}
		} else {
			b.WriteByte(ch)
		}
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

func (l *Lexer) emitOperator() token.Token {
	tok := token.Token{
		Location: token.Location{
			Line:   l.line,
			Column: l.column,
		},
	}
	from := l.pos
	l.advance(LexerModeDefault)
	var typ token.TokenType
	switch l.Source[from] {
	case '+':
		typ = token.Plus
	case '-':
		typ = token.Minus
	case '*':
		typ = token.Multiply
	case '/':
		typ = token.Divide
	case '%':
		typ = token.Modulus
	case '!':
		if l.pos < len(l.Source) && l.Source[l.pos] == '=' {
			typ = token.NotEqual
			l.advance(LexerModeDefault)
		} else {
			typ = token.Not
		}
	case '=':
		typ = token.Equal
	case '&':
		typ = token.And
	case '|':
		typ = token.Or
	case '>':
		if l.pos < len(l.Source) && l.Source[l.pos] == '=' {
			typ = token.GreaterThanOrEqual
			l.advance(LexerModeDefault)
		} else {
			typ = token.GreaterThan
		}
	case '<':
		if l.pos < len(l.Source) && l.Source[l.pos] == '=' {
			typ = token.LessThanOrEqual
			l.advance(LexerModeDefault)
		} else {
			typ = token.LessThan
		}
	default:
		return l.emitError("Unknown operator `" + string(l.Source[from]) + "`")
	}
	tok.Type = typ
	tok.Literal = l.Source[from:l.pos]
	return tok
}

func (l *Lexer) emitParenthesis() token.Token {
	tok := token.Token{
		Location: token.Location{
			Line:   l.line,
			Column: l.column,
		},
	}
	var typ token.TokenType
	switch l.Source[l.pos] {
	case '(':
		typ = token.LParen
	case ')':
		typ = token.RParen
	case '[':
		typ = token.LBracket
	case ']':
		typ = token.RBracket
	default:
		return l.emitError("Syntax error at: `" + string(l.Source[l.pos]) + "`")
	}
	tok.Type = typ
	tok.Literal = string(l.Source[l.pos])
	l.advance(LexerModeDefault)
	return tok
}

func (l *Lexer) emitKWOrIdentifier() token.Token {
	tok := token.Token{
		Location: token.Location{
			Line:   l.line,
			Column: l.column,
		},
	}

	from := l.pos
	to := from
	for !l.isEOF() {
		ch := l.Source[l.pos]
		if isAlpha(ch) || ch == '_' || (to > from && isDigit(ch)) {
			l.advance(LexerModeDefault)
			to++
		} else {
			break
		}
	}

	tok.Literal = l.Source[from:to]
	if kwType, ok := token.Keywords[tok.Literal]; ok {
		tok.Type = kwType
	} else {
		tok.Type = token.Identifier
	}

	if len(tok.Literal) == 0 {
		return l.emitError("Empty identifier or keyword")
	}

	return tok
}

func (l *Lexer) emitAccessor() token.Token {
	tok := token.Token{
		Type: token.Accessor,
		Location: token.Location{
			Line:   l.line,
			Column: l.column,
		},
	}

	from := l.pos
	to := from

	for !l.isEOF() {
		ch := l.Source[l.pos]
		if ch == '.' || isBracket(ch) || isAlpha(ch) || isDigit(ch) {
			l.advance(LexerModeDefault)
			to++
		} else {
			break
		}
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

func (l *Lexer) isOperator() bool {
	if l.isEOF() {
		return false
	}
	ch := l.Source[l.pos]
	return ch == '+' ||
		ch == '-' ||
		ch == '*' ||
		ch == '/' ||
		ch == '%' ||
		ch == '!' ||
		ch == '=' ||
		ch == '&' ||
		ch == '|' ||
		ch == '>' ||
		ch == '<'
}

func (l *Lexer) isParenthesis() bool {
	return !l.isEOF() && (l.Source[l.pos] == '(' ||
		l.Source[l.pos] == ')' ||
		l.Source[l.pos] == '[' ||
		l.Source[l.pos] == ']')
}

func (l *Lexer) isBracket() bool {
	return !l.isEOF() && (l.Source[l.pos] == '[' || l.Source[l.pos] == ']')
}

func (l *Lexer) isKWOrIdentifier() bool {
	return !l.isEOF() && (isAlpha(l.Source[l.pos]) || l.Source[l.pos] == '_')
}

func (l *Lexer) isAccessor() bool {
	return !l.isEOF() && l.Source[l.pos] == '.'
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

func isAlpha(ch byte) bool {
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
}

func isBracket(ch byte) bool {
	return ch == '[' || ch == ']'
}
