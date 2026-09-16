mod lexer;
mod token;
mod range;
mod position;
mod ast;
mod precedence;
mod parser;

use std::fs;
use clap::{Parser};

#[derive(Parser, Debug)]
#[command(author, version, about)]
struct CLIArgs {
    input: String,

    #[arg(short, default_value_t = String::from("out.o"))]
    output: String
}

fn main() -> std::io::Result<()> {
    let args = CLIArgs::parse();

    let source = fs::read_to_string(args.input)?;

    let mut lexer = lexer::Lexer::new("<stdin>", &source);
    let tokens = lexer.lex().unwrap();

    let mut parser = parser::Parser::new(tokens);
    let ast = parser.parse().unwrap();

    println!("{:#?}", ast);
    Ok(())
}
