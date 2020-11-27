void
make_lines(buffer_t *Buf) {
    sb_set_count(Buf->Lines, 0);
    if(Buf->Text.Used == 0) {
        sb_push(Buf->Lines, (line_t){0});
    } else {
        line_t *Line = sb_add(Buf->Lines, 1);
        mem_zero_struct(Line);
        u8 n = 0;
        for(u64 i = 0; i < Buf->Text.Used; i += n) {
            n = utf8_char_width(Buf->Text.Data + i);
            
            Line->Size += n;
            if(Buf->Text.Data[i] == '\n') {
                Line->NewlineSize = n;
                // Push next line and init it
                Line = sb_add(Buf->Lines, 1);
                mem_zero_struct(Line);
                Line->Offset = i + n;
            } else {
                Line->Length += 1;
            }
        }
    }
}

typedef enum {
    TOKEN_Keyword,
    
    TOKEN_CppDirective, // #define, #assert, #if, #error, etc..
    
    TOKEN_SingleLineComment, //
    TOKEN_MultiLineComment, /**/
    
    TOKEN_StringLiteral, // "hello"
    TOKEN_CharLiteral, // 'x'
    
    TOKEN_HexadecimalLiteral, // 0x1234
    TOKEN_DecimalLiteral, // 1234
    TOKEN_OctalLiteral, // 02322
    TOKEN_BinaryLiteral, // 0b10011010010
    
    TOKEN_Bracket, // [] {} (), does not include <>
    
    TOKEN_Division, // / 
    TOKEN_Multiplication, // *
    TOKEN_Modulo, // %
    TOKEN_Addition, // + 
    TOKEN_Subtraction, // -
    TOKEN_LessThan, // < 
    TOKEN_GreaterThan, // >
    
    //TOKEN_Increment, // ++
    //TOKEN_Decrement, // --
    
    //TOKEN_LogicalAnd, // && 
    //TOKEN_LogicalOr, // ||
    //TOKEN_BitwiseAnd, // &
    //TOKEN_BitwiseOr, // |
    TOKEN_And, // &
    TOKEN_Or, // |
    
    TOKEN_LogicalNot, // !
    
    TOKEN_Xor, // ^
    //TOKEN_BitwiseLeftShift, // <<
    //TOKEN_BitwiseRightShift, // >>
    TOKEN_BitwiseNot, // ~
    
    TOKEN_Assignment, // =
    //TOKEN_Comparison, // ==
    
    TOKEN_Dot,
    TOKEN_Semicolon, 
    TOKEN_Comma,
    
} token_type_t;

typedef struct {
    u64 Offset;
    u64 Size;
    token_type_t Type;
    
    union {
        u8 Bracket; // Top bit 1 if open bracket, 0 if closed
        u32 KeywordId;
    };
} token_t;

b8
is_numeric(u8 Char) {
    if(Char >= '0' && Char <= '9') {
        return true;
    } 
    return false;
}

b8
is_alphabetic(u8 Char) {
    if((Char >= 'a' && Char <= 'z') ||
       (Char >= 'A' && Char <= 'Z') ||
       (Char == '_') || (Char >= 128) // >= 128 is utf8 char
       ) {
        return true;
    } 
    return false;
}

b8
is_blank(u8 Char) {
    if(Char == '\t' || Char == ' ' || Char == '\n') {
        return true;
    } 
    return false;
}

u8 *
next_char(buffer_t *Buffer, u8 *Char) {
    u8 *End = Buffer->Text.Data + Buffer->Text.Used;
    u8 *Result = Char + utf8_char_width(Char);
    if(Result < End) {
        return Result;
    }
    
    return 0;
}

b8
c_parse(buffer_t *Buffer, u64 Offset, token_t *Out) {
    b8 HasNext = true;
    token_t Token = {.Offset = Offset};
    if(Buffer->Text.Used > 0) {
        // Find the start of the next token by skipping white space.
        while(Token.Offset < Buffer->Text.Used) {
            u8 *c = &Buffer->Text.Data[Token.Offset];
            if(!is_blank(*c)) {
                break;
            }
            Token.Offset += utf8_char_width(c);
        }
        
        if(Token.Offset < Buffer->Text.Used) {
            u8 *c = &Buffer->Text.Data[Token.Offset];
            
            if(is_alphabetic(*c)) {
                // keyword, identifier
                u64 End = Token.Offset + utf8_char_width(c);
                while(End < Buffer->Text.Used && is_alphabetic(Buffer->Text.Data[End])) {
                    End += utf8_char_width(&Buffer->Text.Data[End]);
                    Token.Type = TOKEN_Keyword;
                }
                Token.Size = End - Token.Offset;
            } else {
                switch(*c) {
                    // Pre-processor directives
                    case '#': {
                        u64 End = Token.Offset + 1;
                        while(End < Buffer->Text.Used && !is_blank(Buffer->Text.Data[End])) {
                            End += utf8_char_width(&Buffer->Text.Data[End]);
                        }
                        Token.Type = TOKEN_CppDirective;
                        Token.Size = (End - Token.Offset);
                    } break;
                    
                    case '/': {
                        u8 *NextChar = next_char(Buffer, c);
                        if(NextChar) {
                            switch(*NextChar) {
                                case '*': {
                                    Token.Type = TOKEN_MultiLineComment;
                                    u64 End = Token.Offset + 2;
                                    for(; End < Buffer->Text.Used; End += utf8_char_width(&Buffer->Text.Data[End])) {
                                        u8 Char = Buffer->Text.Data[End];
                                        if(Char == '/' && Buffer->Text.Data[End - 1] == '*') {
                                            End += 1;
                                            break;
                                        }
                                    }
                                    Token.Size = End - Token.Offset;
                                } break;
                                case '/': {
                                    Token.Type = TOKEN_SingleLineComment;
                                    u64 End = Token.Offset + 2;
                                    for(; End < Buffer->Text.Used; End += utf8_char_width(&Buffer->Text.Data[End])) {
                                        u8 Char = Buffer->Text.Data[End];
                                        if(Char == '\n' && Buffer->Text.Data[End - 1] != '\\') {
                                            break;
                                        }
                                    }
                                    Token.Size = End - Token.Offset;
                                } break;
                                
                                default: {
                                    Token.Type = TOKEN_Division;
                                    Token.Size = 1;
                                }
                            }
                        }
                    } break;
                    
                    case '"': {
                        u64 End = Token.Offset + 1;
                        while(End < Buffer->Text.Used) {
                            if(Buffer->Text.Data[End] == '"') {
                                if(Buffer->Text.Data[End - 1] != '\\') {
                                    End += 1;
                                    break;
                                }
                            }
                            
                            End += utf8_char_width(&Buffer->Text.Data[End]);
                        }
                        Token.Type = TOKEN_StringLiteral;
                        Token.Size = End - Token.Offset;
                    } break;
                    
                    case '\'': {
                        u64 End = Token.Offset + 1;
                        while(End < Buffer->Text.Used) {
                            if(Buffer->Text.Data[End] == '\'') {
                                if(Buffer->Text.Data[End - 1] != '\\') {
                                    End += 1;
                                    break;
                                }
                            }
                            
                            End += utf8_char_width(&Buffer->Text.Data[End]);
                        }
                        Token.Type = TOKEN_CharLiteral;
                        Token.Size = End - Token.Offset;
                    } break;
                    
                    case '0': case '1': case '2': case '3': case '4': 
                    case '5': case '6': case '7': case '8': case '9': {
                        Token.Type = TOKEN_DecimalLiteral;
                        if(*c == '0') {
                            // Could be:
                            // binary literal 0b100101010, 
                            // hexadecimal literal 0x12345
                            // octal literal 027364
                            u8 *NextChar = next_char(Buffer, c);
                            if(NextChar) {
                                switch(*NextChar) {
                                    // Binary
                                    case 'b': { 
                                        Token.Type = TOKEN_BinaryLiteral;
                                    } break;
                                    
                                    // Hexadecimal
                                    case 'x': { 
                                        Token.Type = TOKEN_HexadecimalLiteral;
                                    } break;
                                    
                                    // Octal
                                    case '1': case '2': case '3': case '4': case '5': 
                                    case '6': case '7': case '8': case '9': {
                                        Token.Type = TOKEN_OctalLiteral;
                                    } break;
                                }
                            } 
                        }
                        switch(Token.Type) {
                            case TOKEN_DecimalLiteral: {
                                u64 End = Token.Offset + 1;
                                for(; End < Buffer->Text.Used && is_numeric(Buffer->Text.Data[End]); ++End);
                                Token.Size = End - Token.Offset;
                            } break;
                            
                            case TOKEN_BinaryLiteral: {
                                u64 End = Token.Offset + 2; // 0b followed by 1s and 0s 
                                for(; End < Buffer->Text.Used; ++End) {
                                    u8 Char = Buffer->Text.Data[End];
                                    if(Char != '0' && Char != '1') {
                                        break;
                                    }
                                }
                                Token.Size = End - Token.Offset;
                            } break;
                            
                            case TOKEN_HexadecimalLiteral: {
                                u64 End = Token.Offset + 2; // 0x followed by 0-9 or a-f, ignoring case
                                for(; End < Buffer->Text.Used; ++End) {
                                    u8 Char = Buffer->Text.Data[End];
                                    b32 IsHex = is_numeric(Char) || (Char >= 'a' && Char >= 'f') || (Char >= 'A' && Char >= 'F');
                                    if(!IsHex) {
                                        break;
                                    }
                                }
                                Token.Size = End - Token.Offset;
                            } break;
                            
                            case TOKEN_OctalLiteral: {
                                u64 End = Token.Offset + 2; // 0 followed by 0-7
                                for(; End < Buffer->Text.Used; ++End) {
                                    u8 Char = Buffer->Text.Data[End];
                                    if(Char < '0' || Char > '7') {
                                        break;
                                    }
                                }
                                Token.Size = End - Token.Offset;
                            } break;
                        }
                    } break;
                    
                    // Bracket, 
                    case '[': Token.Bracket = 0x80; goto parse_bracket;
                    case ']': Token.Bracket = 0; goto parse_bracket;
                    
                    case '{': Token.Bracket = 0x80; goto parse_bracket;
                    case '}': Token.Bracket = 0; goto parse_bracket;
                    
                    case '(': Token.Bracket = 0x80; goto parse_bracket;
                    case ')': Token.Bracket = 0; goto parse_bracket;
                    
                    parse_bracket: {
                        Token.Type = TOKEN_Bracket;
                        Token.Size = 1;
                        Token.Bracket |= (*c) & 0x7f;
                    } break;
                    
                    // Operators
                    case '*': { Token.Type = TOKEN_Multiplication; Token.Size = 1; } break;
                    case '%': { Token.Type = TOKEN_Modulo;         Token.Size = 1; } break;
                    case '+': { Token.Type = TOKEN_Addition;       Token.Size = 1; } break;
                    case '<': { Token.Type = TOKEN_LessThan;       Token.Size = 1; } break;
                    case '>': { Token.Type = TOKEN_GreaterThan;    Token.Size = 1; } break;
                    case '-': { Token.Type = TOKEN_Subtraction;    Token.Size = 1; } break;
                    case '&': { Token.Type = TOKEN_And;            Token.Size = 1; } break;
                    case '!': { Token.Type = TOKEN_LogicalNot;     Token.Size = 1; } break;
                    case '^': { Token.Type = TOKEN_Xor;            Token.Size = 1; } break;
                    case '~': { Token.Type = TOKEN_BitwiseNot;     Token.Size = 1; } break;
                    case '=': { Token.Type = TOKEN_Assignment;     Token.Size = 1; } break;
                    case '.': { Token.Type = TOKEN_Dot;            Token.Size = 1; } break;
                    case ';': { Token.Type = TOKEN_Semicolon;      Token.Size = 1; } break;
                    case ',': { Token.Type = TOKEN_Comma;          Token.Size = 1; } break;
                    
                    
                    default: {
                        HasNext = false;
                    } break;
                }
            }
        }
    } else {
        HasNext = false;
    }
    
    *Out = Token;
    
    return HasNext;
}