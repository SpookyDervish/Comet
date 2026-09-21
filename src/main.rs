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
mod comet_error;

use std::fs;
use std::io::Write;
use clap::{Parser};
use miette::{IntoDiagnostic};

#[derive(Parser, Debug)]
#[command(author, version, about)]
struct CLIArgs {
    input: String,

    #[arg(short, default_value_t = String::from("out.o"))]
    output: String
}

fn main() -> miette::Result<()> {

    let args = CLIArgs::parse();

    let file_name = args.input;
    let source = fs::read_to_string(&file_name).into_diagnostic()?;

    let mut lexer = lexer::Lexer::new(file_name.as_str(), &source);

    let tokens = lexer.lex()?;

    let mut parser = parser::Parser::new(tokens);
    let ast = parser.parse()?;

    //println!("{:#?}", ast);

    let mut compiler = compiler::Compiler::new(&file_name, source.clone()).unwrap();
    compiler.compile(&ast, None)?;

    let bytes = compiler.end_module().unwrap();

    let mut file = std::fs::File::create(args.output).into_diagnostic()?;
    file.write_all(&bytes).into_diagnostic()?;

    Ok(())
}
