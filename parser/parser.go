package parser

import (
	"fmt"
	"osdrv/liss/ast"
	"osdrv/liss/lexer"
	"osdrv/liss/token"
)

type Parser struct {
	lex       *lexer.Lexer
	pos       int         // current position in the token stream
	curToken  token.Token // current token being processed
	nextToken token.Token // next token to be processed
	isEOF     bool        // flag to indicate if the end of file has been reached
}

func NewParser(lex *lexer.Lexer) *Parser {
	return &Parser{
		lex:   lex,
		isEOF: false,
	}
}

func (p *Parser) Parse() (*ast.Program, error) {
	nodes := make([]ast.Node, 0)

	// pre-populate the first two tokens
	for range 2 {
		if err := p.advance(); err != nil {
			return nil, err
		}
	}

	for !p.isEOF {
		expr, err := p.parseNode()
		if err != nil {
			return nil, err
		}
		nodes = append(nodes, expr)
	}

	program := ast.NewProgram(nodes)
	return program, nil
}

func (p *Parser) advance() error {
	tok := p.lex.NextToken()
	if tok.Type == token.Error {
		return p.lex.Err()
	}
	p.curToken = p.nextToken
	p.nextToken = tok
	if p.curToken.Type == token.EOF {
		p.isEOF = true
	}
	return nil
}

func (p *Parser) consume(expected token.TokenType) error {
	if p.curToken.Type != expected {
		return fmt.Errorf("expected token type %s, got %s", expected, p.curToken.Type)
	}
	if err := p.advance(); err != nil {
		return err
	}
	return nil
}

func (p *Parser) peek() token.Token {
	if p.isEOF {
		return token.Token{Type: token.EOF}
	}
	return p.nextToken
}

func (p *Parser) parseNode() (ast.Node, error) {
	var node ast.Node
	var err error
	switch p.curToken.Type {
	case token.True, token.False:
		node, err = ast.NewBooleanLiteral(p.curToken)
	case token.Null:
		node, err = ast.NewNullLiteral(p.curToken)
	case token.Numeric:
		node, err = p.parseNumericLiteral(p.curToken)
	case token.String:
		node, err = ast.NewStringLiteral(p.curToken)
	case token.Identifier:
		node, err = ast.NewIdentifierExpr(p.curToken)
	case token.Plus,
		token.Minus,
		token.Multiply,
		token.Divide,
		token.Modulus,
		token.Equal,
		token.NotEqual,
		token.LessThan,
		token.LessThanOrEqual,
		token.GreaterThan,
		token.GreaterThanOrEqual,
		token.And,
		token.Or,
		token.Not:
		node, err = ast.NewOperatorExpr(p.curToken)
	case token.LParen:
		node, err = p.parseExpression()
	default:
		return nil, fmt.Errorf("unexpected token type: %s", p.curToken.Type.String())
	}
	p.advance()
	return node, err
}

func (p *Parser) parseExpression() (ast.Node, error) {
	if err := p.consume(token.LParen); err != nil {
		return nil, err
	}
	nodes := make([]ast.Node, 0, 1)
	for !p.isEOF {
		if p.curToken.Type == token.RParen {
			if err := p.advance(); err != nil {
				return nil, err
			}
			break
		}
		node, err := p.parseNode()
		if err != nil {
			return nil, err
		}
		nodes = append(nodes, node)
	}

	node := ast.NewExpression(nodes)
	return node, nil
}

func (p *Parser) parseNumericLiteral(tok token.Token) (ast.Node, error) {
	if tok.Type != token.Numeric {
		return nil, fmt.Errorf("expected token type Numeric, got %s", tok.Type.String())
	}
	if looksLikeInteger(tok) {
		return ast.NewIntegerLiteral(tok)
	}
	return ast.NewFloatLiteral(tok)
}

func looksLikeInteger(tok token.Token) bool {
	if len(tok.Literal) == 0 {
		return false
	}
	for ix, r := range tok.Literal {
		if r == '+' || r == '-' {
			if ix != 0 {
				return false
			}
		} else if r < '0' || r > '9' {
			return false
		}
	}
	return true
}
