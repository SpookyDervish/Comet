use crate::precedence::PrecedenceType;
use crate::token::{Token, TokenType};
use crate::ast::{ASTNode, ASTNodeType};

pub struct Parser {
    tokens: Vec<Token>,
    token_index: usize,
}

impl Parser {
    pub fn new(tokens: Vec<Token>) -> Self {
        Parser {
            tokens: tokens,
            token_index: 0,
        }
    }

    // HELPER METHODS //
    fn current_token(&self) -> Option<&Token> {
        self.tokens.get(self.token_index)
    }

    fn peek_token(&self) -> Option<&Token> {
        self.tokens.get(self.token_index + 1)
    }

    fn advance_token(&mut self) {
        self.token_index += 1;
    }

    fn peek_token_is(&self, token_type: &TokenType) -> bool {
        return self.peek_token().is_some() && (std::mem::discriminant(self.peek_token().unwrap().token_type()) == std::mem::discriminant(token_type));
    }

    fn expect_peek(&mut self, token_type: TokenType) -> Result<(), String> {
        if self.peek_token_is(&token_type) {
            self.advance_token();
            return Ok(());
        }

        let peek = self.peek_token();

        if peek.is_none() {
            Err(format!("Expected next token to be {:?}, got <EOF> instead.", token_type))
        } else {
            Err(format!("Expected next token to be {:?}, got {:?} instead.", token_type, peek.unwrap().token_type()))
        }
    }

    fn current_precedence(&self) -> PrecedenceType {
        let current_tok = self.current_token();
        if current_tok.is_none() {
            return PrecedenceType::Lowest;
        }

        current_tok.unwrap().token_type().precedence()
    }

    fn peek_precedence(&self) -> PrecedenceType {
        let peek_tok = self.peek_token();
        if peek_tok.is_none() {
            return PrecedenceType::Lowest;
        }

        peek_tok.unwrap().token_type().precedence()
    }

    fn get_prefix_parse_func(&self, token_type: &TokenType) -> Option<fn(&mut Parser) -> Result<ASTNode, String>> {
        match token_type {
            // literals
            TokenType::IntLiteral(_) => Some(Parser::parse_int_literal),
            TokenType::FloatLiteral(_) => Some(Parser::parse_float_literal),
            TokenType::Identifier(_) => Some(Parser::parse_identifier_literal),
            _ => None
        }
    }

    fn get_infix_parse_func(&self, token_type: &TokenType) -> Option<fn(&mut Parser, left_node: ASTNode) -> Result<ASTNode, String>> {
        match token_type {
            TokenType::Plus => Some(Parser::parse_infix_expression),
            TokenType::Minus => Some(Parser::parse_infix_expression),
            TokenType::Times => Some(Parser::parse_infix_expression),
            TokenType::Divide => Some(Parser::parse_infix_expression),
            _ => None
        }
    }
    // END OF HELPER METHODS //

    // STATEMENT METHODS //
    fn parse_statement(&mut self) -> Result<ASTNode, String> {
        match self.current_token().unwrap().token_type() {
            TokenType::Func => self.parse_func_def_statement(),
            _ => self.parse_expression_statement()
        }
    }

    fn parse_expression_statement(&mut self) -> Result<ASTNode, String> {
        let expr = self.parse_expression(PrecedenceType::Lowest)?;
        let stmt = ASTNode::new(ASTNodeType::ExpressionStatement(Box::new(expr)));
        Ok(stmt)
    }

    fn parse_block_statement(&mut self) -> Result<ASTNode, String> {
        self.expect_peek(TokenType::OpenCurly)?;

        self.advance_token();

        let mut statements: Vec<ASTNode> = Vec::new();

        while self.peek_token().is_some() {
            if self.peek_token_is(&TokenType::CloseCurly) {
                break;
            }

            statements.push(self.parse_statement()?);
        }

        Ok(ASTNode::new(ASTNodeType::Block(statements)))
    }

    fn parse_type(&mut self) -> Result<ASTNode, String> {
        self.expect_peek(TokenType::Identifier(String::new()))?;

        // best rust code EVER
        Ok(ASTNode::new(ASTNodeType::TypeLiteral(Box::new(ASTNode::new(ASTNodeType::IdentifierLiteral(self.current_token().unwrap().token_type().as_identifier().cloned().unwrap()))))))
    }

    fn parse_func_def_args(&mut self) -> Result<Vec<ASTNode>, String> {
        let mut args = Vec::new();

        if self.peek_token_is(&TokenType::CloseParen) {
            self.advance_token();
            return Ok(args);
        }

        while self.peek_token().is_some() {
            self.expect_peek(TokenType::Identifier(String::new()))?;

            let arg_name = self.current_token()
                .and_then(|token| token.token_type().as_identifier())
                .cloned()
                .ok_or_else(|| String::from("Expected an identifier argument"))?;

            self.expect_peek(TokenType::Colon)?;
            
            let type_ = self.parse_type()?;

            args.push(ASTNode::new(ASTNodeType::FuncArgDefinition { 
                name: Box::new(ASTNode::new(ASTNodeType::IdentifierLiteral(arg_name))),
                type_: Box::new(type_) 
            }));

            if self.peek_token_is(&TokenType::Comma) {
                self.advance_token();
                continue;
            }

            self.expect_peek(TokenType::CloseParen)?;
            break;
        }

        Ok(args)
    }

    fn parse_func_def_statement(&mut self) -> Result<ASTNode, String> {
        /*

        - Example syntax: -

        func add(a: i32, b: i32) :: i32 {
            /// code here
        }

         */
        

        // parse func name
        self.expect_peek(TokenType::Identifier(String::new()))?;
        let name = self.current_token().unwrap().token_type().as_identifier().cloned().unwrap();

        // parse func args
        self.expect_peek(TokenType::OpenParen)?;
        let args = self.parse_func_def_args()?;

        // check if function has return type
        let mut return_type: Option<Box<ASTNode>> = None;
        if self.peek_token_is(&TokenType::ColonColon) {
            // parse return type
            self.advance_token();
            return_type = Some(Box::new(self.parse_type()?));
        }

        // parse function body
        let body = self.parse_block_statement()?;

        // return
        let stmt = ASTNode::new(ASTNodeType::FuncDefinitionStatement { 
            name: Box::new(ASTNode::new(ASTNodeType::IdentifierLiteral(name))),
            args: args,
            return_type: return_type,
            body: Box::new(body) 
        });
        Ok(stmt)
    }
    // END OF STATEMENT METHODS //

    // EXPRESSION METHODS //
    fn parse_expression(&mut self, precedence: PrecedenceType) -> Result<ASTNode, String> {
        let prefix_fn = self.get_prefix_parse_func(self.current_token().unwrap().token_type());
        if prefix_fn.is_none() {
            return Err(format!("No prefix function for {:?}", self.current_token().unwrap().token_type()));
        }

        let mut left_expr = prefix_fn.unwrap()(self)?;

        while precedence < self.peek_precedence() {
            let infix_fn = self.get_infix_parse_func(self.peek_token().unwrap().token_type());
            if infix_fn.is_none() {
                return Err(format!("No infix function for {:?}", self.peek_token().unwrap().token_type()));
            }

            self.advance_token();

            left_expr = infix_fn.unwrap()(self, left_expr)?;
        }

        Ok(left_expr)
    }

    fn parse_infix_expression(&mut self, left_node: ASTNode) -> Result<ASTNode, String> {

        let op = self.current_token().unwrap().clone();

        let precedence = self.current_precedence();

        self.advance_token();

        let right_node = self.parse_expression(precedence)?;

        Ok(ASTNode::new(ASTNodeType::InfixExpression {
            left: Box::new(left_node),
            op: op,
            right: Box::new(right_node)
        }))
    }

    fn parse_grouped_expression(&mut self) -> Result<ASTNode, String> {
        self.advance_token();

        let expr = self.parse_expression(PrecedenceType::Lowest);
        self.expect_peek(TokenType::CloseParen)?;

        expr
    }
    // END OF EXPRESSION METHODS //

    // PREFIX METHODS //
    fn parse_int_literal(&mut self) -> Result<ASTNode, String> {
        let token = self.current_token().unwrap();

        match token.token_type() {
            TokenType::IntLiteral(value) => {
                let result = value.parse::<u64>();
                if result.is_err() {
                    return Err(String::from("failed to parse int"));
                }

                return Ok(ASTNode::new(ASTNodeType::IntLiteral(result.unwrap())));
            }
            _ => unreachable!(),
        }
    }

    fn parse_float_literal(&mut self) -> Result<ASTNode, String> {
        let token = self.current_token().unwrap();

        match token.token_type() {
            TokenType::FloatLiteral(value) => {
                let result = value.parse::<f64>();
                if result.is_err() {
                    return Err(String::from("failed to parse float"));
                }

                return Ok(ASTNode::new(ASTNodeType::FloatLiteral(result.unwrap())));
            }
            _ => unreachable!(),
        }
    }

    fn parse_identifier_literal(&mut self) -> Result<ASTNode, String> {
        let token = self.current_token().unwrap();

        match token.token_type() {
            TokenType::Identifier(value) => {
                return Ok(ASTNode::new(ASTNodeType::IdentifierLiteral(value.clone())));
            }
            _ => unreachable!(),
        }
    }
    // END OF PREFIX METHODS //
    

    pub fn parse(&mut self) -> Result<ASTNode, String> {
        let mut root = ASTNode::new(ASTNodeType::Program(Vec::new()));

        while self.peek_token().is_some() {
            let stmt_result = self.parse_statement();

            match stmt_result {
                Ok(stmt) => {
                    match root.node_type_mut() {
                        ASTNodeType::Program(stmts) => stmts.push(stmt),
                        _ => unreachable!()
                    }
                }

                Err(e) => { return Err(e); }
            }

            self.advance_token();
        }

        Ok(root)
    }

    //
}