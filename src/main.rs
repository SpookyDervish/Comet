mod lexer;
mod token;
mod range;
mod position;
mod ast;
mod precedence;
mod parser;
mod compiler;
mod scope;
mod comet_type;
mod comet_struct;

use std::fs;
use std::io::Write;
use clap::{Parser};
use miette::miette;

#[derive(Parser, Debug)]
#[command(author, version, about)]
struct CLIArgs {
    input: String,

    #[arg(short, default_value_t = String::from("out.o"))]
    output: String
}

fn main() {

    let args = CLIArgs::parse();

    let source = fs::read_to_string(args.input).unwrap();

    let mut lexer = lexer::Lexer::new("<stdin>", &source);

    let tokens = lexer.lex();
    if let Err(err) = tokens {
        println!("{}", miette!(err));
        return;
    }

    let mut parser = parser::Parser::new(tokens.unwrap());
    let ast = parser.parse();
    if let Err(err) = ast {
        println!("{}", miette!(err));
        return;
    }
    let ast = ast.unwrap();

    //println!("{:#?}", ast);

    let mut compiler = compiler::Compiler::new().unwrap();
    let compile_result = compiler.compile(&ast, None);
    if let Err(err) = compile_result {
        println!("{}", miette!(err));
        return;
    }

    let bytes = compiler.end_module().unwrap();

    let mut file = std::fs::File::create(args.output);
    file.unwrap().write_all(&bytes).unwrap();
}
