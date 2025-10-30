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

func (p *Parser) Parse() (ast.Node, error) {
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

	program := ast.NewBlockExpression(nodes)
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
		node, err = p.parseBooleanLiteral()
	case token.Null:
		node, err = p.parseNullLiteral()
	case token.Numeric:
		node, err = p.parseNumericLiteral()
	case token.String:
		node, err = p.parseStringLiteral()
	case token.Identifier:
		node, err = p.parseIdentifierExpr()
	case token.LParen:
		node, err = p.parseExpression()
	case token.LBracket:
		node, err = p.parseListExpression()
	default:
		return nil, fmt.Errorf(
			"unexpected token type: %s at %s",
			p.curToken.Type.String(),
			p.curToken.Pos())
	}
	return node, err
}

func (p *Parser) parseFunctionExpression() (ast.Node, error) {
	tok := p.curToken
	if err := p.consume(token.Fn); err != nil {
		return nil, err
	}
	var name *ast.IdentifierExpr
	if p.curToken.Type == token.Identifier {
		nn, err := p.parseIdentifierExpr()
		if err != nil {
			return nil, err
		}
		name = nn.(*ast.IdentifierExpr)
	}
	if err := p.consume(token.LBracket); err != nil {
		return nil, err
	}
	args := make([]*ast.IdentifierExpr, 0)
	for p.curToken.Type != token.RBracket {
		arg, err := p.parseIdentifierExpr()
		if err != nil {
			return nil, err
		}
		args = append(args, arg.(*ast.IdentifierExpr))
	}
	if err := p.consume(token.RBracket); err != nil {
		return nil, err
	}
	body := make([]ast.Node, 0)
	for !p.isEOF && p.curToken.Type != token.RParen {
		node, err := p.parseNode()
		if err != nil {
			return nil, err
		}
		body = append(body, node)
	}
	return ast.NewFunctionExpression(tok, name, args, body)
}

func (p *Parser) parseIdentifierExpr() (ast.Node, error) {
	tok := p.curToken
	if err := p.advance(); err != nil {
		return nil, err
	}
	return ast.NewIdentifierExpr(tok)
}

func (p *Parser) parseLetExpression() (ast.Node, error) {
	tok := p.curToken
	if err := p.consume(token.Let); err != nil {
		return nil, err
	}
	id, err := p.parseIdentifierExpr()
	if err != nil {
		return nil, err
	}
	val, err := p.parseNode()
	if err != nil {
		return nil, err
	}

	return ast.NewLetExpression(tok, id.(*ast.IdentifierExpr), val)
}

func (p *Parser) parseImportExpression() (ast.Node, error) {
	tok := p.curToken
	if err := p.consume(token.Import); err != nil {
		return nil, err
	}
	if p.curToken.Type != token.String {
		return nil, fmt.Errorf("import expression requires a string path as argument")
	}
	ref, err := p.parseStringLiteral()
	if err != nil {
		return nil, err
	}
	return ast.NewImportExpression(tok, ref)
}

func (p *Parser) parseCondExpression() (ast.Node, error) {
	tok := p.curToken
	if err := p.consume(token.Cond); err != nil {
		return nil, err
	}
	cond, err := p.parseNode()
	if err != nil {
		return nil, err
	}
	th, err := p.parseNode()
	if err != nil {
		return nil, err
	}
	var el ast.Node
	if p.curToken.Type != token.RParen {
		el, err = p.parseNode()
		if err != nil {
			return nil, err
		}
	}
	if p.curToken.Type != token.RParen {
		return nil, fmt.Errorf("too many operands for cond expression")
	}
	return ast.NewCondExpression(tok, cond, th, el)
}

func (p *Parser) parseListExpression() (ast.Node, error) {
	tok := p.curToken
	if err := p.consume(token.LBracket); err != nil {
		return nil, err
	}
	elements := make([]ast.Node, 0)
	for p.curToken.Type != token.RBracket {
		elem, err := p.parseNode()
		if err != nil {
			return nil, err
		}
		elements = append(elements, elem)
	}
	if err := p.consume(token.RBracket); err != nil {
		return nil, err
	}
	return ast.NewListExpression(tok, elements)
}

func (p *Parser) parseExpression() (ast.Node, error) {
	if err := p.consume(token.LParen); err != nil {
		return nil, err
	}

	var node ast.Node
	var err error

	switch p.curToken.Type {
	case token.Cond:
		node, err = p.parseCondExpression()
	case token.Fn:
		node, err = p.parseFunctionExpression()
	case token.Let:
		node, err = p.parseLetExpression()
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
		node, err = p.parseOperatorExpression()
	case token.Import:
		node, err = p.parseImportExpression()
	default:
		tok := p.curToken
		nodes := make([]ast.Node, 0, 1)
		for !p.isEOF {
			if p.curToken.Type == token.RParen {
				break
			}
			node, err := p.parseNode()
			if err != nil {
				return nil, err
			}
			nodes = append(nodes, node)
		}
		if len(nodes) > 0 {
			switch nodes[0].(type) {
			case *ast.IdentifierExpr, *ast.FunctionExpression:
				node, err = ast.NewCallExpression(tok, nodes[0], nodes[1:])
			default:
				node = ast.NewBlockExpression(nodes)
			}
		} else {
			node = ast.NewBlockExpression(nodes)
		}
	}

	if err != nil {
		return nil, err
	}
	if err := p.consume(token.RParen); err != nil {
		return nil, err
	}
	return node, err
}

func (p *Parser) parseStringLiteral() (ast.Node, error) {
	tok := p.curToken
	if err := p.advance(); err != nil {
		return nil, err
	}
	return ast.NewStringLiteral(tok)
}

func (p *Parser) parseNumericLiteral() (ast.Node, error) {
	tok := p.curToken
	if err := p.advance(); err != nil {
		return nil, err
	}
	if looksLikeInteger(tok) {
		return ast.NewIntegerLiteral(tok)
	}
	return ast.NewFloatLiteral(tok)
}

func (p *Parser) parseOperatorExpression() (ast.Node, error) {
	tok := p.curToken
	if err := p.advance(); err != nil {
		return nil, err
	}
	args := make([]ast.Node, 0)
	for p.curToken.Type != token.RParen {
		arg, err := p.parseNode()
		if err != nil {
			return nil, err
		}
		args = append(args, arg)
	}
	return ast.NewOperatorExpr(tok, args)
}

func (p *Parser) parseNullLiteral() (ast.Node, error) {
	tok := p.curToken
	if err := p.advance(); err != nil {
		return nil, err
	}
	return ast.NewNullLiteral(tok)
}

func (p *Parser) parseBooleanLiteral() (ast.Node, error) {
	tok := p.curToken
	if err := p.advance(); err != nil {
		return nil, err
	}
	return ast.NewBooleanLiteral(tok)
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
