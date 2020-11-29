u64 KeywordHashes[] = {
    0xe7599c190572c560, 0x7f2b6c605332dd30, 0x2138d5192571b731, 0x2587bcef32493841, 0x5dc77784260a7236, 0x915ea6605dc15d80, 0x554667f2165fec5d, 0x5a5fe3720c9584cf, 0xcd2fd49bc6b014bd, 0xf7b24048d2701d49, 0xc5a11c2dd9ab8cec, 0x5b5c98ef514dbfa5, 0x93b7591debc7ce38, 0xedb2d39755161f73, 0x38fe7925726e5cbd, 0x570ac119447423ee, 0xb55e6190e8792fd1, 0x5cc53b62ea063f7, 0xf4b72165f60f5b60, 0xe51f408ea2730be4, 0xc1b2e33b13ec076c, 0xb5fae2c14238b978, 0x134dcf77c0b3eea0, 0x690b0e3f4c3bef38, 0xf2a393910b5b3ebd, 0xa00a62a942b20165, 0xe10e210d0e407655, 0x91853f297dfecb6e, 0xd11655952fcbab9f, 0xdcb27818fed9da90, 0xc5c7b983377cad5f, 0x4b2a584e9a909034, 0x65c9718e19e3df34, 0xb501d7a879f0854d, 0xf6915548f781cd55, 0xb7b1a6ed402882e6, 0xcb762a9f4cf6f292, 0x9d40d1720eeae746, 0x5055c58f7a753b55, 0xd9bcbc712744e027, 0xd2bfd5accd1966a4, 0x8b73007b55c3e26, 0x8a4a6236192e319d, 0x3a8bdbd8e17b835c, 0xebada5168620c5fe, 0xe43c3b14f8fd3d4, 0xc534816d6d11e97b, 0x3173c900e37ae1df, 0xf3fe6b5fdb85d50a, 0x2b9fff192bd4c83e, 0x384157e47f809f43, 0x9575f08fb5a48f0d, 0x8915907b53bb494, 0xcde8c9ad70d16733, 0xc318a9d898991720, 0xcd074885fe311c91, 0xa0880a9ce131dea8, 0xd3ac3c45566efde9, 0xa5a87ac5b0b379b1, 0xce87a3885811296e, 0xb706dc12aa9d9f5c, 0xdc1c4a04cb5c6da0, 0x33609a5e9eb92f4b, 0xf999b7199ff422e6, 0x6f34c0c3c2ddfeed, 0x3108fe3d785c2b3d, 0xbfa9b2197fe7163e, 0x1e5b266ba57eb071, 0x6d36868e1dbec272, 0x8b05407b5565ca4, 0x557847ce6cc35ba9, 0x598feba3d9002de9, 0x215ad619258e9f4a, 0x6ca17d73a48b7847
};

u64
fnv1a_64(range_t String) {
    u64 Hash = 0xcbf29ce484222325;
    for(u64 i = 0; i < String.Size; ++i) {
        Hash ^= String.Data[i];
        Hash *= 0x100000001b3;
    }
    return Hash;
}

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
    TOKEN_Unknown = 0,
    
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
    TOKEN_FloatLiteral,
    
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
        u64 KeywordHash;
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
next_token(buffer_t *Buffer, u64 Offset, token_t *Out) {
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
                for(; End < Buffer->Text.Used; End += utf8_char_width(&Buffer->Text.Data[End])) {
                    u8 Char = Buffer->Text.Data[End];
                    if(!is_alphabetic(Char) && !is_numeric(Char)) {
                        break;
                    }
                }
                Token.Type = TOKEN_Keyword;
                Token.Size = End - Token.Offset;
                Token.KeywordHash = fnv1a_64((range_t){.Data = Buffer->Text.Data + Token.Offset, .Size = Token.Size});
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
                                // Could be decimal number or float
                                u64 End = Token.Offset + 1;
                                
                                for(; End < Buffer->Text.Used; ++End) {
                                    u8 Char = Buffer->Text.Data[End];
                                    if(!is_numeric(Char)) {
                                        if((Char == '.') || (Char == 'e')) {
                                            Token.Type = TOKEN_FloatLiteral;
                                        }
                                        break;
                                    }
                                }
                                
                                if(Token.Type == TOKEN_FloatLiteral) {
                                    b32 DecimalPointHasBeenPresent = false;
                                    b32 ExponentHasBeenPresent = false;
                                    for(; End < Buffer->Text.Used; ++End) {
                                        u8 Char = Buffer->Text.Data[End];
                                        if(Char == '.') {
                                            if(DecimalPointHasBeenPresent) {
                                                break;
                                            } else {
                                                DecimalPointHasBeenPresent = true;
                                            }
                                        } else if (Char == 'e') {
                                            if(ExponentHasBeenPresent) {
                                                break;
                                            } else {
                                                ExponentHasBeenPresent = true;
                                            }
                                        } else {
                                            if(!is_numeric(Char)) {
                                                if(Char != 'f') {
                                                    break;
                                                } else {
                                                    End += 1;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
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
                                    b32 IsHex = is_numeric(Char) || (Char >= 'a' && Char <= 'f') || (Char >= 'A' && Char <= 'F');
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
                    
                    case '.': {
                        // Preemptively assume it's just a dot.
                        Token.Type = TOKEN_Dot;
                        Token.Size = 1;
                        
                        u8 *NextChar = next_char(Buffer, c);
                        if(NextChar) {
                            // Floating point number written as .3 or .3f
                            if(is_numeric(*NextChar)) {
                                u64 End = Token.Offset + 2;
                                b32 ExponentHasBeenPresent = false;
                                for(; End < Buffer->Text.Used; ++End) {
                                    u8 Char = Buffer->Text.Data[End];
                                    if(Char == '.') {
                                        break;
                                    } else if (Char == 'e') {
                                        if(ExponentHasBeenPresent) {
                                            break;
                                        } else {
                                            ExponentHasBeenPresent = true;
                                        }
                                    } else {
                                        if(!is_numeric(Char)) {
                                            if(Char != 'f') {
                                                break;
                                            } else {
                                                End += 1;
                                                break;
                                            }
                                        }
                                    }
                                }
                                Token.Size = End - Token.Offset;
                                Token.Type = TOKEN_FloatLiteral;
                            }
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
                    case ';': { Token.Type = TOKEN_Semicolon;      Token.Size = 1; } break;
                    case ',': { Token.Type = TOKEN_Comma;          Token.Size = 1; } break;
                    
                    default: {
                        Token.Type = TOKEN_Unknown;
                        Token.Size = utf8_char_width(c);
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