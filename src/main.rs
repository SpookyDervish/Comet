mod lexer;
mod token;
mod range;
mod error;
mod position;

fn main() {
    let source = String::from("var x = 123");
    let mut lexer = lexer::Lexer::new("<stdin>", &source);
    let tokens = lexer.lex().unwrap();

    for tok in &tokens {
        println!("{:?}", tok);
    }
}
