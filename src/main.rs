mod lexer;
mod token;
mod range;
mod position;
mod ast;
mod precedence;
mod parser;

fn main() {
    let source = String::from("1 + 2 * 2");
    let mut lexer = lexer::Lexer::new("<stdin>", &source);
    let tokens = lexer.lex().unwrap();

    let mut parser = parser::Parser::new(tokens);
    let ast = parser.parse().unwrap();

    println!("{:#?}", ast);
}
