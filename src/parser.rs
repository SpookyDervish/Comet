use miette::NamedSource;

use crate::comet_error::{NotAFunction, SyntaxError};
use crate::precedence::PrecedenceType;
use crate::range::Range;
use crate::token::{Token, TokenType};
use crate::ast::{ASTNode, ASTNodeType, ASTType};

pub struct Parser <'a> {
    tokens: Vec<Token<'a>>,
    token_index: usize,
}

impl <'a> Parser <'a> {
    pub fn new(tokens: Vec<Token<'a>>) -> Self {
        Parser {
            tokens: tokens,
            token_index: 0,
        }
    }

    // HELPER METHODS //
    fn current_token(&self) -> Option<&Token<'a>> {
        self.tokens.get(self.token_index)
    }

    fn peek_token(&self) -> Option<&Token<'a>> {
        self.tokens.get(self.token_index + 1)
    }

    fn advance_token(&mut self) {
        self.token_index += 1;
    }

    fn step_back_token(&mut self) {
        self.token_index -= 1;
    }

    fn peek_token_is(&self, token_type: &TokenType) -> bool {
        return self.peek_token().is_some() && (std::mem::discriminant(self.peek_token().unwrap().token_type()) == std::mem::discriminant(token_type));
    }

    fn current_token_is(&self, token_type: &TokenType) -> bool {
        return self.current_token().is_some() && (std::mem::discriminant(self.current_token().unwrap().token_type()) == std::mem::discriminant(token_type));
    }

    fn expect_peek(&mut self, token_type: TokenType) -> miette::Result<()> {
        if self.peek_token_is(&token_type) {
            self.advance_token();
            return Ok(());
        }

        let peek = self.peek_token();

        let curr = self.current_token().unwrap();
        let pos = curr.pos();

        let err_src = NamedSource::new(pos.file_name(), String::from(pos.source()));

        if peek.is_none() {
            Err(SyntaxError {
                span: curr.end_span(),
                src: err_src,
                text: format!("Expected next token to be {:?}, got <EOF> instead.", token_type)
            }.into())
        } else {
            Err(SyntaxError {
                span: curr.end_span(),
                src: err_src,
                text: format!("Expected next token to be {:?}, got {:?} instead.", token_type, peek.unwrap().token_type())
            }.into())
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

    fn get_prefix_parse_func(&self, token_type: &TokenType) -> Option<fn(&mut Parser<'a>) -> miette::Result<ASTNode<'a>>> {
        match token_type {
            // literals
            TokenType::IntLiteral(_) => Some(Parser::parse_int_literal),
            TokenType::FloatLiteral(_) => Some(Parser::parse_float_literal),
            TokenType::StringLiteral(_) => Some(Parser::parse_string_literal),
            TokenType::Identifier(_) => Some(Parser::parse_identifier_literal),
            TokenType::OpenSquare => Some(Parser::parse_array_literal),
            TokenType::New => Some(Parser::parse_struct_create_expr),

            // prefix exprs
            TokenType::Not => Some(Parser::parse_prefix_expr),
            TokenType::Ampersand => Some(Parser::parse_prefix_expr),
            TokenType::Caret => Some(Parser::parse_prefix_expr),
            TokenType::Times => Some(Parser::parse_prefix_expr),

            TokenType::OpenParen => Some(Parser::parse_grouped_expression),
            _ => None
        }
    }

    fn get_infix_parse_func(&self, token_type: &TokenType) -> Option<fn(&mut Parser<'a>, left_node: ASTNode<'a>) -> miette::Result<ASTNode<'a>>> {
        match token_type {
            TokenType::Plus => Some(Parser::parse_infix_expression),
            TokenType::Minus => Some(Parser::parse_infix_expression),
            TokenType::Times => Some(Parser::parse_infix_expression),
            TokenType::Divide => Some(Parser::parse_infix_expression),
            TokenType::Modulo => Some(Parser::parse_infix_expression),
            TokenType::EqEq => Some(Parser::parse_infix_expression),
            TokenType::NotEq => Some(Parser::parse_infix_expression),
            TokenType::Lt => Some(Parser::parse_infix_expression),
            TokenType::Gt => Some(Parser::parse_infix_expression),
            TokenType::LtEq => Some(Parser::parse_infix_expression),
            TokenType::GtEq => Some(Parser::parse_infix_expression),
            TokenType::Dot => Some(Parser::parse_infix_expression),
            TokenType::ColonColon => Some(Parser::parse_infix_expression),

            TokenType::OpenSquare => Some(Parser::parse_index_expression),

            TokenType::OpenParen => Some(Parser::parse_func_call),
            _ => None
        }
    }
    // END OF HELPER METHODS //

    // STATEMENT METHODS //
    fn parse_statement(&mut self) -> miette::Result<ASTNode<'a>> {
        match self.current_token().unwrap().token_type() {
            TokenType::Fun => self.parse_func_def_statement(),
            TokenType::Ret => self.parse_ret_statement(),
            TokenType::Match => self.parse_match_statement(),
            TokenType::If => self.parse_if_statement(),
            TokenType::While => self.parse_while_statement(),
            TokenType::Struct => self.parse_struct_def_statement(),
            TokenType::Imp => self.parse_impl_block(),

            TokenType::Hash => self.parse_compiler_directive(),

            _ => self.parse_expression_or_assignment_statement()
        }
    }

    fn parse_expression_or_assignment_statement(&mut self) -> miette::Result<ASTNode<'a>> {
        if self.current_token_is(&TokenType::Identifier(String::new()))
            && self.peek_token_is(&TokenType::Colon)
        {
            let ident = self.parse_identifier_literal()?;

            self.advance_token(); // skip ':'
            let type_ = self.parse_type()?;

            self.expect_peek(TokenType::Eq)?;
            self.advance_token(); // skip '='

            let value = self.parse_expression(PrecedenceType::Lowest)?;
            let span = ident.get_span(&value);
            self.advance_token();

            return Ok(ASTNode::new(ASTNodeType::AssignStatement {
                ident: Box::new(ident),
                type_: Some(Box::new(type_)),
                value: Box::new(value)
            }, span));
        }

        let left = self.parse_expression(PrecedenceType::Lowest)?;
        

        if self.peek_token_is(&TokenType::Eq) {
            self.advance_token(); // skip '='
            self.advance_token(); // skip first value token

            let value = self.parse_expression(PrecedenceType::Lowest)?;
            let span = left.get_span(&value);
            self.advance_token();

            return Ok(ASTNode::new(ASTNodeType::AssignStatement {
                ident: Box::new(left),
                type_: None,
                value: Box::new(value)
            }, span))

        }

        self.advance_token();

        let span = left.source_span().clone();
        Ok(ASTNode::new(ASTNodeType::ExpressionStatement(Box::new(left)), span))
    }

    fn parse_block_statement(&mut self) -> miette::Result<ASTNode<'a>> {
        self.expect_peek(TokenType::OpenCurly)?;
        let mut block_range = Range::start(self.current_token().unwrap().pos());

        self.advance_token();

        let mut statements: Vec<ASTNode> = Vec::new();

        while !self.current_token_is(&TokenType::CloseCurly) {
            statements.push(self.parse_statement()?);
        }

        block_range.end(self.current_token().unwrap().end_pos());

        Ok(ASTNode::new(ASTNodeType::Block(statements), block_range.source_span()))
    }

    // TYPE PARSING //
    fn parse_function_type(&mut self) -> miette::Result<ASTType<'a>> {
        let mut range = Range::start(self.current_token().unwrap().pos());

        self.expect_peek(TokenType::Fun)?;
        self.expect_peek(TokenType::OpenParen)?;

        let mut arg_types: Vec<ASTNode> = Vec::new();

        while !self.peek_token_is(&TokenType::CloseParen) {
            let arg_type = self.parse_type()?;
            arg_types.push(arg_type);

            if self.peek_token_is(&TokenType::CloseParen) {
                break;
            }

            self.expect_peek(TokenType::Comma)?;
            self.advance_token();
        }

        self.advance_token();

        let mut return_type = None;
        if self.peek_token_is(&TokenType::ColonColon) {
            self.advance_token();

            return_type = Some(Box::new(self.parse_type()?));
        }

        range.end(self.current_token().unwrap().end_pos());

        Ok(ASTType::Function { arg_types: arg_types, return_type: return_type })
    }

    fn parse_generic_instance_types(&mut self) -> miette::Result<Vec<ASTNode<'a>>> {
        self.advance_token(); // skip '<'

        let mut generic_types = Vec::new();

        while !self.peek_token_is(&TokenType::Gt) {
            let type_ = self.parse_type()?;
            generic_types.push(type_);


            if self.peek_token_is(&TokenType::Comma) {
                self.advance_token();
                continue;
            }

            self.expect_peek(TokenType::Gt)?;
            break;
        }

        return Ok(generic_types);
    }

    fn parse_type(&mut self) -> miette::Result<ASTNode<'a>> {
        let mut range = Range::start(self.current_token().unwrap().pos());

        let base_type;
        if self.peek_token_is(&TokenType::Ampersand) {
            self.advance_token();

            let inner_type = self.parse_type()?;
            base_type = ASTType::Pointer(Box::new(inner_type));

        } else if self.peek_token_is(&TokenType::Fun) {
            base_type = self.parse_function_type()?;
        } else if self.peek_token_is(&TokenType::OpenSquare) {
            self.advance_token();

            let inner_type = self.parse_type()?;

            self.expect_peek(TokenType::Comma)?;
            self.advance_token();

            let array_length = self.parse_int_literal()?;

            base_type = ASTType::Array(Box::new(inner_type), Box::new(array_length));

            self.expect_peek(TokenType::CloseSquare)?;
        } else {
            self.expect_peek(TokenType::Identifier(String::new()))?;

            let curr = &self.current_token().unwrap();

            let ident_node = ASTNode::new(
                ASTNodeType::IdentifierLiteral(curr.token_type().as_identifier().cloned().unwrap()),
                curr.source_span()
            );

            base_type = ASTType::Identifier(Box::new(ident_node));
        }

        
        let generic_types = if self.peek_token_is(&TokenType::Lt) {
            Some(self.parse_generic_instance_types()?)
        } else {
            None
        };
        range.end(self.current_token().unwrap().end_pos());

        Ok(ASTNode::new(ASTNodeType::TypeLiteral { base_type: base_type, generic_types: generic_types }, range.source_span()))
    }
    // END OF TYPE PARSING //

    fn parse_func_def_args(&mut self) -> miette::Result<Vec<ASTNode<'a>>> {
        let mut args = Vec::new();

        if self.peek_token_is(&TokenType::CloseParen) {
            self.advance_token();
            return Ok(args);
        }

        while self.peek_token().is_some() {
            let mut arg_range = Range::start(self.current_token().unwrap().pos());
            self.expect_peek(TokenType::Identifier(String::new()))?;

            
            let name_span = self.current_token().unwrap().source_span();

            let arg_name = self.current_token()
                .and_then(|token| token.token_type().as_identifier())
                .cloned()
                .unwrap();

            self.expect_peek(TokenType::Colon)?;
            
            let type_ = self.parse_type()?;
            arg_range.end(self.current_token().unwrap().end_pos());

            args.push(ASTNode::new(ASTNodeType::FuncArgDefinition { 
                name: Box::new(ASTNode::new(ASTNodeType::IdentifierLiteral(arg_name), name_span)),
                type_: Box::new(type_) 
            }, arg_range.source_span()));

            if self.peek_token_is(&TokenType::Comma) {
                self.advance_token();
                continue;
            }

            self.expect_peek(TokenType::CloseParen)?;
            break;
        }

        Ok(args)
    }

    fn parse_func_def_statement(&mut self) -> miette::Result<ASTNode<'a>> {
        /*

        - Example syntax: -

        func add(a: i32, b: i32) :: i32 {
            /// code here
        }

         */
        
        let mut func_range = Range::start(self.current_token().unwrap().pos());

        // parse func name
        self.expect_peek(TokenType::Identifier(String::new()))?;
        let name = self.current_token().unwrap().token_type().as_identifier().cloned().unwrap();
        let name_range = self.current_token().unwrap().source_span();

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

        func_range.end(self.current_token().unwrap().end_pos());

        // return
        let stmt = ASTNode::new(ASTNodeType::FuncDefinitionStatement { 
            name: Box::new(ASTNode::new(ASTNodeType::IdentifierLiteral(name), name_range)),
            args: args,
            return_type: return_type,
            body: Box::new(body) 
        }, func_range.source_span());
        Ok(stmt)
    }

    fn parse_ret_statement(&mut self) -> miette::Result<ASTNode<'a>> {
        let mut ret_range = Range::start(self.current_token().unwrap().pos());

        self.advance_token();

        let result = match self.current_token() {
            Some(token) if self.get_prefix_parse_func(token.token_type()).is_some() => {
                let result = self.parse_expression(PrecedenceType::Lowest)?;
                ret_range.end(self.current_token().unwrap().end_pos());
                self.advance_token();
                Some(Box::new(result))
            },
            Some(token) => {
                ret_range.end(token.pos());
                None
            },
            None => None
        };

        Ok(ASTNode::new(ASTNodeType::ReturnStatement(result), ret_range.source_span()))
    }

    fn parse_match_statement(&mut self) -> miette::Result<ASTNode<'a>> {
        let mut nodes: Vec<ASTNode> = vec![];

        let mut match_range = Range::start(self.current_token().unwrap().pos());

        self.advance_token(); // skip 'match'

        let match_expr = self.parse_expression(PrecedenceType::Lowest)?;

        self.expect_peek(TokenType::OpenCurly)?;

        self.advance_token(); // skip '{'

        let mut default_branch: Option<Box<ASTNode>> = None;

        loop {
            let mut expressions: Vec<ASTNode> = vec![];

            // default branch
            if self.current_token_is(&TokenType::Default) {
                let block = self.parse_block_statement()?;

                default_branch = Some(Box::new(block));
            } else { // expressions
        
                let mut expressions_range = Range::start(self.current_token().unwrap().pos());

                loop {
                    let node_expr = self.parse_expression(PrecedenceType::Lowest)?;
                    expressions.push(node_expr);

                    

                    if self.peek_token_is(&TokenType::OpenCurly) {
                        break;
                    }

                    self.expect_peek(TokenType::Or)?;

                    self.advance_token(); // skip 'or'
                }

                let block = self.parse_block_statement()?;

                expressions_range.end(self.current_token().unwrap().end_pos());

                nodes.push(ASTNode::new(ASTNodeType::MatchNode { expressions: expressions, block: Box::new(block) }, expressions_range.source_span()));

            }

            


            
            if self.peek_token_is(&TokenType::CloseCurly) {
                break;
            }

            self.expect_peek(TokenType::Comma)?;

            self.advance_token();
        }

        self.expect_peek(TokenType::CloseCurly)?;

        match_range.end(self.current_token().unwrap().end_pos());

        self.advance_token(); // skip extra '}'

        Ok(ASTNode::new(ASTNodeType::MatchStatement { expr: Box::new(match_expr), nodes: nodes, default: default_branch }, match_range.source_span()))
    }

    fn parse_if_statement(&mut self) -> miette::Result<ASTNode<'a>> {
        let mut if_range = Range::start(self.current_token().unwrap().pos());

        self.advance_token(); // skip 'if'

        let expr = self.parse_expression(PrecedenceType::Lowest)?;
        let body = self.parse_block_statement()?;


        let mut else_body: Option<Box<ASTNode>> = None;

        if self.peek_token_is(&TokenType::Else) {
            self.advance_token();

            if self.peek_token_is(&TokenType::OpenCurly) {
                else_body = Some(Box::new(self.parse_block_statement()?));
            } else {
                self.expect_peek(TokenType::If)?;
                else_body = Some(Box::new(self.parse_if_statement()?));
            }
        }

        if_range.end(self.current_token().unwrap().end_pos());

        self.advance_token();

        Ok(ASTNode::new(ASTNodeType::IfStatement { expr: Box::new(expr), body: Box::new(body), else_body: else_body }, if_range.source_span()))
    }

    fn parse_while_statement(&mut self) -> miette::Result<ASTNode<'a>> {
        let mut while_range = Range::start(self.current_token().unwrap().pos());

        self.advance_token(); // skip 'while'

        let expr = self.parse_expression(PrecedenceType::Lowest)?;
        let body = self.parse_block_statement()?;

        self.advance_token();

        while_range.end(self.current_token().unwrap().end_pos());

        Ok(ASTNode::new(ASTNodeType::WhileStatement { expr: Box::new(expr), body: Box::new(body) }, while_range.source_span()))
    }

    fn parse_generic_def(&mut self) -> miette::Result<Vec<ASTNode<'a>>> {
        self.advance_token(); // skip '<'

        let mut generic_type_names = Vec::new();

        while !self.peek_token_is(&TokenType::Gt) {
            self.expect_peek(TokenType::Identifier(String::new()))?;

            let generic_name = self.parse_identifier_literal()?;
            generic_type_names.push(generic_name);

            if self.peek_token_is(&TokenType::Comma) {
                self.advance_token();
                continue;
            }

            self.expect_peek(TokenType::Gt)?;
            break;
        }

        Ok(generic_type_names)
    }

    fn parse_struct_def_statement(&mut self) -> miette::Result<ASTNode<'a>> {
        let mut struct_range = Range::start(self.current_token().unwrap().pos());

        self.expect_peek(TokenType::Identifier(String::new()))?;

        let ident = self.parse_identifier_literal()?;

        let generic_args = if self.peek_token_is(&TokenType::Lt) {
            Some(self.parse_generic_def()?)
        } else {
            None
        };
        

        self.expect_peek(TokenType::OpenCurly)?;

        let mut fields: Vec<ASTNode> = Vec::new();

        while !self.peek_token_is(&TokenType::CloseCurly) {
            self.expect_peek(TokenType::Identifier(String::new()))?;

            let mut field_range = Range::start(self.current_token().unwrap().pos());
            let field_ident = self.parse_identifier_literal()?;

            self.expect_peek(TokenType::Colon)?;

            let field_type = self.parse_type()?;
            field_range.end(self.current_token().unwrap().end_pos());

            fields.push(ASTNode::new(
                ASTNodeType::StructFieldDefinition { ident: Box::new(field_ident), type_: Box::new(field_type) },
                field_range.source_span()
            ));
        
            if self.peek_token_is(&TokenType::Comma) {
                self.advance_token();
                continue;
            }

            self.expect_peek(TokenType::CloseCurly)?;
            break;
        }

        struct_range.end(self.current_token().unwrap().end_pos());

        Ok(ASTNode::new(ASTNodeType::StructDefinitionStatement { ident: Box::new(ident), fields: fields, generics: generic_args }, struct_range.source_span()))
    }

    fn parse_impl_block(&mut self) -> miette::Result<ASTNode<'a>> {
        let mut range = Range::start(self.current_token().unwrap().pos());
        
        let struct_type = self.parse_type()?;

        self.expect_peek(TokenType::OpenCurly)?;
        self.advance_token();

        let mut functions = Vec::new();

        while !self.current_token_is(&TokenType::CloseCurly) {
            let function = self.parse_statement()?;

            match function.node_type() {
                ASTNodeType::FuncDefinitionStatement { name: _, args: _, return_type: _, body: _ } => {},
                _ => {
                    let pos = self.current_token().unwrap().pos();

                    return Err(NotAFunction {
                        func_name: String::new(),
                        span: function.source_span(),
                        src: NamedSource::new(pos.file_name(), String::from(pos.source()))
                    }.into()) ;
                }
            }

            functions.push(function);
        }

        self.advance_token();

        range.end(self.current_token().unwrap().end_pos());

        Ok(ASTNode::new(ASTNodeType::ImplDefStatement { struct_type: Box::new(struct_type), functions: functions }, range.source_span()))
    }

    fn parse_compiler_directive(&mut self) -> miette::Result<ASTNode<'a>> {
        let mut range = Range::start(self.current_token().unwrap().pos());

        self.expect_peek(TokenType::Identifier(String::new()))?;
        let directive = self.parse_identifier_literal()?;

        self.expect_peek(TokenType::Identifier(String::new()))?;

        let value_name = self.parse_identifier_literal()?;

        self.advance_token();

        let value_type = self.parse_type()?;

        range.end(self.current_token().unwrap().end_pos());

        Ok(ASTNode::new(ASTNodeType::CompilerDirectiveStatement { directive: Box::new(directive), value_name: Box::new(value_name), value_type: Box::new(value_type) }, range.source_span()))
    }
    // END OF STATEMENT METHODS //

    // EXPRESSION METHODS //
    fn parse_expression(&mut self, precedence: PrecedenceType) -> miette::Result<ASTNode<'a>> {
        let curr = &self.current_token().unwrap();
        let prefix_fn = self.get_prefix_parse_func(curr.token_type());
        if prefix_fn.is_none() {
            let pos = curr.pos();

            return Err(SyntaxError {
                span: curr.source_span(),
                src: NamedSource::new(pos.file_name(), String::from(pos.source())),
                text: format!("No prefix function for {:?}", curr.token_type())
            }.into());
        }

        let mut left_expr = prefix_fn.unwrap()(self)?;

        while precedence < self.peek_precedence() {
            let peek = &self.peek_token().unwrap();
            let infix_fn = self.get_infix_parse_func(peek.token_type());
            if infix_fn.is_none() {
                let pos = peek.pos();

                return Err(SyntaxError {
                    span: peek.source_span(),
                    src: NamedSource::new(pos.file_name(), String::from(pos.source())),
                    text: format!("No infix function for {:?}", peek.token_type())
                }.into());
            }

            self.advance_token();

            left_expr = infix_fn.unwrap()(self, left_expr)?;
        }

        Ok(left_expr)
    }

    fn parse_infix_expression(&mut self, left_node: ASTNode<'a>) -> miette::Result<ASTNode<'a>> {

        let op = self.current_token().unwrap().clone();
        let mut expr_range = Range::start(self.current_token().unwrap().pos());

        let precedence = self.current_precedence();

        self.advance_token();

        let right_node = self.parse_expression(precedence)?;
        expr_range.end(self.current_token().unwrap().end_pos());

        Ok(ASTNode::new(ASTNodeType::InfixExpression {
            left: Box::new(left_node),
            op: op,
            right: Box::new(right_node)
        }, expr_range.source_span()))
    }

    fn parse_prefix_expr(&mut self) -> miette::Result<ASTNode<'a>> {
        let mut range = Range::start(self.current_token().unwrap().pos());

        let op = self.current_token().unwrap().clone();

        self.advance_token();

        let right_expr = self.parse_expression(PrecedenceType::Prefix)?;

        range.end(self.current_token().unwrap().end_pos());

        return Ok(ASTNode::new(ASTNodeType::PrefixExpression { op: op, right: Box::new(right_expr) }, range.source_span()));
    }

    fn parse_func_call(&mut self, left_node: ASTNode<'a>) -> miette::Result<ASTNode<'a>> {
        let mut func_call_range = Range::start(self.current_token().unwrap().pos());

        let mut func_call_args: Vec<ASTNode> = Vec::new();

        let mut should_loop = true;
        if self.peek_token_is(&TokenType::CloseParen) {
            self.advance_token();
            should_loop = false;
        }
        
        while should_loop {
            self.advance_token();

            let arg = self.parse_expression(PrecedenceType::Lowest)?;
            func_call_args.push(arg);

            if self.peek_token_is(&TokenType::Comma) {
                self.advance_token();
                continue;
            }

            self.expect_peek(TokenType::CloseParen)?;
            break;
        }

        func_call_range.end(self.current_token().unwrap().end_pos());

        Ok(ASTNode::new(ASTNodeType::FuncCall { left: Box::new(left_node), args: func_call_args }, func_call_range.source_span() ))
    }

    fn parse_index_expression(&mut self, left_node: ASTNode<'a>) -> miette::Result<ASTNode<'a>> {
        let mut range = Range::start(self.current_token().unwrap().pos());

        self.advance_token();

        let index = self.parse_expression(PrecedenceType::Lowest)?;

        self.expect_peek(TokenType::CloseSquare)?;

        range.end(self.current_token().unwrap().end_pos());

        Ok(ASTNode::new(ASTNodeType::IndexExpression { left: Box::new(left_node), index: Box::new(index) }, range.source_span()))
    }

    fn parse_grouped_expression(&mut self) -> miette::Result<ASTNode<'a>> {
        self.advance_token();

        let expr = self.parse_expression(PrecedenceType::Lowest)?;
        self.expect_peek(TokenType::CloseParen)?;

        Ok(expr)
    }

    fn parse_struct_create_expr(&mut self) -> miette::Result<ASTNode<'a>> {
        let mut struct_create_range = Range::start(self.current_token().unwrap().pos());

        let type_ = self.parse_type()?;
        self.expect_peek(TokenType::OpenCurly)?;

        

        let mut fields: Vec<ASTNode> = Vec::new();

        while !self.peek_token_is(&TokenType::CloseCurly) {
            self.expect_peek(TokenType::Identifier(String::new()))?;

            let mut struct_field_range = Range::start(self.current_token().unwrap().pos());

            let field_ident = self.parse_identifier_literal()?;

            self.expect_peek(TokenType::Eq)?;
            self.advance_token();

            let value = self.parse_expression(PrecedenceType::Lowest)?;
            struct_field_range.end(self.current_token().unwrap().end_pos());

            fields.push(ASTNode::new(ASTNodeType::StructField { ident: Box::new(field_ident), value: Box::new(value) }, struct_field_range.source_span()));

            if self.peek_token_is(&TokenType::Comma) {
                self.advance_token();
                continue;
            }

            self.expect_peek(TokenType::CloseCurly)?;

            break;
        }

        struct_create_range.end(self.current_token().unwrap().end_pos());

        Ok(ASTNode::new(ASTNodeType::StructCreateExpression { type_: Box::new(type_), fields: fields }, struct_create_range.source_span()))
    }
    // END OF EXPRESSION METHODS //

    // PREFIX METHODS //
    fn parse_int_literal(&mut self) -> miette::Result<ASTNode<'a>> {
        let token = self.current_token().cloned().unwrap();

        match token.token_type() {
            TokenType::IntLiteral(value) => {
                let result = value.parse::<u64>();
                if result.is_err() {
                    let pos = token.pos();

                    return Err(SyntaxError {
                        span: token.source_span(),
                        src: NamedSource::new(pos.file_name(), String::from(pos.source())),
                        text: format!("failed to parse '{:?}' as an integer", token.token_type())
                    }.into());
                }

                return Ok(ASTNode::new(ASTNodeType::IntLiteral(result.unwrap()), token.source_span()));
            }
            _ => unreachable!(),
        }
    }

    fn parse_float_literal(&mut self) -> miette::Result<ASTNode<'a>> {
        let token = self.current_token().cloned().unwrap();

        match token.token_type() {
            TokenType::FloatLiteral(value) => {
                let result = value.parse::<f64>();
                if result.is_err() {
                    let pos = token.pos();

                    return Err(SyntaxError {
                        span: token.source_span(),
                        src: NamedSource::new(pos.file_name(), String::from(pos.source())),
                        text: format!("failed to parse '{:?}' as a float", token.token_type())
                    }.into());
                }

                return Ok(ASTNode::new(ASTNodeType::FloatLiteral(result.unwrap()), token.source_span()));
            }
            _ => unreachable!(),
        }
    }

    fn parse_string_literal(&mut self) -> miette::Result<ASTNode<'a>> {
        let token = self.current_token().cloned().unwrap();

        match token.token_type() {
            TokenType::StringLiteral(value) => {
                return Ok(ASTNode::new(ASTNodeType::StringLiteral(value.clone()), token.source_span()));
            }
            _ => unreachable!(),
        }
    }

    fn parse_identifier_literal(&mut self) -> miette::Result<ASTNode<'a>> {
        let token = self.current_token().cloned().unwrap();

        match token.token_type() {
            TokenType::Identifier(value) => {
                return Ok(ASTNode::new(ASTNodeType::IdentifierLiteral(value.clone()), token.source_span()));
            }
            _ => unreachable!(),
        }
    }

    fn parse_array_literal(&mut self) -> miette::Result<ASTNode<'a>> {
        

        let mut range = Range::start(self.current_token().unwrap().pos());

        let mut elements = Vec::new();

        self.advance_token();

        while !self.current_token_is(&TokenType::CloseSquare) {

            let elem = self.parse_expression(PrecedenceType::Lowest)?;
            elements.push(elem);

            if self.peek_token_is(&TokenType::Comma) {
                self.advance_token();
                self.advance_token(); // skip ','
                continue;
            }

            self.expect_peek(TokenType::CloseSquare)?;
            break;
        }


        range.end(self.current_token().unwrap().end_pos());

        Ok(ASTNode::new(ASTNodeType::ArrayLiteral(elements), range.source_span()))
    }
    // END OF PREFIX METHODS //
    

    pub fn parse(&mut self) -> miette::Result<ASTNode<'a>> {
        let mut program_range = Range::new(0, 0, 0, 0, 0, 0);
        let mut stmts = Vec::new();

        let mut last_token_pos = None;
        while self.peek_token().is_some() {
            last_token_pos = self.peek_token().map(|token| token.pos().clone()) ;
            let stmt = self.parse_statement()?;
            stmts.push(stmt);
            self.advance_token();

            
        }

        if let Some(last_token_pos) = last_token_pos {
            program_range.end(&last_token_pos);
        }
        let root = ASTNode::new(ASTNodeType::Program(stmts), program_range.source_span());

        Ok(root)
    }

    //
}