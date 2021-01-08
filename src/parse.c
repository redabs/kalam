typedef enum {
    KEYWORD_asm = 1,
    KEYWORD_else,
    KEYWORD_new,
    KEYWORD_this,
    KEYWORD_auto,
    KEYWORD_enum,
    KEYWORD_operator,
    KEYWORD_throw,
    KEYWORD_bool,
    KEYWORD_explicit,
    KEYWORD_private,
    KEYWORD_true,
    KEYWORD_break,
    KEYWORD_export,
    KEYWORD_protected,
    KEYWORD_try,
    KEYWORD_case,
    KEYWORD_extern,
    KEYWORD_public,
    KEYWORD_typedef,
    KEYWORD_catch,
    KEYWORD_false,
    KEYWORD_register,
    KEYWORD_typeid,
    KEYWORD_char,
    KEYWORD_float,
    KEYWORD_reinterpret_cast,
    KEYWORD_typename,
    KEYWORD_class,
    KEYWORD_for,
    KEYWORD_return,
    KEYWORD_union,
    KEYWORD_const,
    KEYWORD_friend,
    KEYWORD_short,
    KEYWORD_unsigned,
    KEYWORD_const_cast,
    KEYWORD_goto,
    KEYWORD_signed,
    KEYWORD_using,
    KEYWORD_continue,
    KEYWORD_if,
    KEYWORD_sizeof,
    KEYWORD_virtual,
    KEYWORD_default,
    KEYWORD_inline,
    KEYWORD_static,
    KEYWORD_void,
    KEYWORD_delete,
    KEYWORD_int,
    KEYWORD_static_cast,
    KEYWORD_volatile,
    KEYWORD_do,
    KEYWORD_long,
    KEYWORD_struct,
    KEYWORD_wchar_t,
    KEYWORD_double,
    KEYWORD_mutable,
    KEYWORD_switch,
    KEYWORD_while,
    KEYWORD_dynamic_cast,
    KEYWORD_namespace,
    KEYWORD_template,
    KEYWORD_and,
    KEYWORD_bitor,
    KEYWORD_not_eq,
    KEYWORD_xor,
    KEYWORD_and_eq,
    KEYWORD_compl,
    KEYWORD_or,
    KEYWORD_xor_eq,
    KEYWORD_bitand,
    KEYWORD_not,
    KEYWORD_or_eq,
    
    CPPDIRECTIVE_assert,
    CPPDIRECTIVE_define,
    CPPDIRECTIVE_elif,
    CPPDIRECTIVE_else,
    CPPDIRECTIVE_endif,
    CPPDIRECTIVE_error,
    CPPDIRECTIVE_ident,
    CPPDIRECTIVE_if,
    CPPDIRECTIVE_ifdef,
    CPPDIRECTIVE_ifndef,
    CPPDIRECTIVE_import,
    CPPDIRECTIVE_include,
    CPPDIRECTIVE_include_next,
    CPPDIRECTIVE_line,
    CPPDIRECTIVE_pragma,
    CPPDIRECTIVE_sccs,
    CPPDIRECTIVE_unassert,
    CPPDIRECTIVE_undef,
    CPPDIRECTIVE_warning,
    
    CPP_PREDEF_MAX,
} cpp_keyword_type_t, cpp_directive_type_t;

u64 CppPredefHashes[CPP_PREDEF_MAX] = {
    0, 0xe7599c190572c560, 0x7f2b6c605332dd30, 0x2138d5192571b731, 0x2587bcef32493841, 0x5dc77784260a7236, 0x915ea6605dc15d80, 0x554667f2165fec5d, 0x5a5fe3720c9584cf, 0xcd2fd49bc6b014bd, 0xf7b24048d2701d49, 0xc5a11c2dd9ab8cec, 0x5b5c98ef514dbfa5, 0x93b7591debc7ce38, 0xedb2d39755161f73, 0x38fe7925726e5cbd, 0x570ac119447423ee, 0xb55e6190e8792fd1, 0x5cc53b62ea063f7, 0xf4b72165f60f5b60, 0xe51f408ea2730be4, 0xc1b2e33b13ec076c, 0xb5fae2c14238b978, 0x134dcf77c0b3eea0, 0x690b0e3f4c3bef38, 0xf2a393910b5b3ebd, 0xa00a62a942b20165, 0xe10e210d0e407655, 0x91853f297dfecb6e, 0xd11655952fcbab9f, 0xdcb27818fed9da90, 0xc5c7b983377cad5f, 0x4b2a584e9a909034, 0x65c9718e19e3df34, 0xb501d7a879f0854d, 0xf6915548f781cd55, 0xb7b1a6ed402882e6, 0xcb762a9f4cf6f292, 0x9d40d1720eeae746, 0x5055c58f7a753b55, 0xd9bcbc712744e027, 0xd2bfd5accd1966a4, 0x8b73007b55c3e26, 0x8a4a6236192e319d, 0x3a8bdbd8e17b835c, 0xebada5168620c5fe, 0xe43c3b14f8fd3d4, 0xc534816d6d11e97b, 0x3173c900e37ae1df, 0xf3fe6b5fdb85d50a, 0x2b9fff192bd4c83e, 0x384157e47f809f43, 0x9575f08fb5a48f0d, 0x8915907b53bb494, 0xcde8c9ad70d16733, 0xc318a9d898991720, 0xcd074885fe311c91, 0xa0880a9ce131dea8, 0xd3ac3c45566efde9, 0xa5a87ac5b0b379b1, 0xce87a3885811296e, 0xb706dc12aa9d9f5c, 0xdc1c4a04cb5c6da0, 0x33609a5e9eb92f4b, 0xe6f79719051ff286, 0x6f34c0c3c2ddfeed, 0x3108fe3d785c2b3d, 0xbfa9b2197fe7163e, 0x1e5b266ba57eb071, 0x6d36868e1dbec272, 0x8b05407b5565ca4, 0x557847ce6cc35ba9, 0x598feba3d9002de9, 0x215ad619258e9f4a, 0x6ca17d73a48b7847, 0xe5fa41199584a158, 0xcaacbf9bb2bdb94d, 0x87b47ae7e47b5eb0, 0x879289e7e45e91c7, 0xc46b4f1a2a853aac, 0xbd70f873b2728fd4, 0x915c06ca0dd8041c, 0xcd52a917d46dfb25, 0xda33cddabf5b2ccc, 0x6b431a068c4c0588, 0xcfc287309225a9cf, 0x1237c3cc4a3b9012, 0x42abc43bd655ea76, 0xb4713123458252a0, 0x4635f63b0358f1c2, 0x622bb42f2062664e, 0xae92f47dd56a0487, 0xceb0ae86a9c2c408, 0xf04062bfafba2bde, 
};

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
    TOKEN_Identifier,
    
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
    TOKEN_Star, // *
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
    
    TOKEN_IncludePath,
    
    TOKEN_EscapedNewline,
} token_type_t;

typedef struct {
    u64 Offset;
    u64 Size;
    token_type_t Type;
    
    // This is non-zero if this token is either a pre-processor directive or syntactically part of one.
    u64 CppDirectiveHash; // CppPredefHashes[CPPDIRECTIVE_*] 
    
    union {
        u8 Bracket; // Top bit 1 if open bracket, 0 if closed
        u64 Hash;
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
is_keyword_or_identifier_char(u8 Char) {
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
parse_string_or_char_literal(buffer_t *Buffer, u64 Offset, token_t *Out) {
    u8 Symbol = Buffer->Text.Data[Offset];
    if(Symbol != '"' && Symbol != '\'') {
        return false;
    }
    
    u64 End = Offset + 1;
    for(; End < Buffer->Text.Used; End += utf8_char_width(&Buffer->Text.Data[End])) {
        u8 *Char = &Buffer->Text.Data[End];
        if(*Char == Symbol || *Char == '\n') {
            s32 Count = 0;
            for(u8 *c = Char - 1; c >= Buffer->Text.Data && *c == '\\'; --c) {
                ++Count;
            }
            if((Count & 1) == 0) {
                End += 1;
                break;
            }
        } 
    }
    
    Out->Offset = Offset;
    Out->Type = (Symbol == '"') ? TOKEN_StringLiteral : TOKEN_CharLiteral;
    Out->Size = End - Offset;
    return true;
}

// Set Previous = {0} to get the first token.
b8
next_token(buffer_t *Buffer, token_t Previous, token_t *Out) {
    b8 HasNext = true;
    token_t Token = {.Offset = Previous.Offset + Previous.Size, .CppDirectiveHash = Previous.CppDirectiveHash};
    
    // There can't be multiple include paths
    if(Previous.Type == TOKEN_IncludePath) {
        Token.CppDirectiveHash = 0;
    }
    HasNext = Buffer->Text.Used > 0;
    if(HasNext) {
        // Find the start of the next token by skipping white space.
        while(Token.Offset < Buffer->Text.Used) {
            u8 *Char = &Buffer->Text.Data[Token.Offset];
            
            if(!is_blank(*Char)) {
                b32 IsEscapedNewlineCharacter = (*Char == '\\' && Token.Offset < Buffer->Text.Used && *(Char + 1) == '\n');
                if(IsEscapedNewlineCharacter) {
                    Token.Size = 2;
                    Token.Type = TOKEN_EscapedNewline;
                    *Out = Token;
                    return true;
                } else {
                    break;
                }
            } else if(*Char == '\n') {
                Token.CppDirectiveHash = 0;
            }
            
            Token.Offset += utf8_char_width(Char);
        }
        HasNext = Token.Offset < Buffer->Text.Used;
        if(HasNext) {
            u8 *Cursor = &Buffer->Text.Data[Token.Offset];
            
            if(is_keyword_or_identifier_char(*Cursor)) {
                // keyword, identifier
                u64 End = Token.Offset + utf8_char_width(Cursor);
                for(; End < Buffer->Text.Used; End += utf8_char_width(&Buffer->Text.Data[End])) {
                    u8 Char = Buffer->Text.Data[End];
                    if(!is_keyword_or_identifier_char(Char) && !is_numeric(Char)) {
                        break;
                    }
                }
                Token.Size = End - Token.Offset;
                Token.Hash = fnv1a_64((range_t){.Data = Buffer->Text.Data + Token.Offset, .Size = Token.Size}); 
                Token.Type = TOKEN_Identifier;
                for(int i = 0; i < ARRAY_COUNT(CppPredefHashes); ++i) {
                    if(CppPredefHashes[i] == Token.Hash) {
                        Token.Type = TOKEN_Keyword;
                    }
                }
            } else {
                switch(*Cursor) {
                    // Pre-processor directives
                    case '#': {
                        u64 End = Token.Offset + 1;
                        while(End < Buffer->Text.Used && !is_blank(Buffer->Text.Data[End])) {
                            End += utf8_char_width(&Buffer->Text.Data[End]);
                        }
                        Token.Type = TOKEN_CppDirective;
                        Token.Size = (End - Token.Offset);
                        
                        // TODO: For speed we could to this in the loop above, if needed.
                        Token.Hash = fnv1a_64((range_t){.Data = Buffer->Text.Data + Token.Offset, .Size = Token.Size});
                        Token.CppDirectiveHash = Token.Hash;
                    } break;
                    
                    case '/': {
                        u8 *NextChar = next_char(Buffer, Cursor);
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
                                        u8 *Char = &Buffer->Text.Data[End];
                                        if(*Char == '\n' && *(Char - 1) != '\\') {
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
                        if(Token.CppDirectiveHash == CppPredefHashes[CPPDIRECTIVE_include] || Token.CppDirectiveHash == CppPredefHashes[CPPDIRECTIVE_include_next]) {
                            u64 End = Token.Offset + 1;
                            for(; End < Buffer->Text.Used; End += utf8_char_width(&Buffer->Text.Data[End])) {
                                u8 Char = Buffer->Text.Data[End];
                                if(Char == '"') {
                                    End += 1;
                                    Token.Type = TOKEN_IncludePath;
                                    break;
                                } else if(Char == '\n') {
                                    Token.Type = TOKEN_Unknown;
                                    break;
                                }
                            }
                            Token.Size = End - Token.Offset;
                        } else {
                            parse_string_or_char_literal(Buffer, Token.Offset, &Token);
                        }
                    } break;
                    
                    case '\'': { 
                        parse_string_or_char_literal(Buffer, Token.Offset, &Token);
                    } break;
                    
                    case '0': case '1': case '2': case '3': case '4': 
                    case '5': case '6': case '7': case '8': case '9': {
                        Token.Type = TOKEN_DecimalLiteral;
                        if(*Cursor == '0') {
                            // Could be:
                            // binary literal 0b100101010, 
                            // hexadecimal literal 0x12345
                            // octal literal 027364
                            u8 *NextChar = next_char(Buffer, Cursor);
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
                        
                        u8 *NextChar = next_char(Buffer, Cursor);
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
                    
                    // Bracket
                    case '[':
                    case ']':
                    case '{':
                    case '}':
                    case '(':
                    case ')': {
                        Token.Type = TOKEN_Bracket;
                        Token.Size = 1;
                        Token.Bracket = (*Cursor);
                    } break;
                    
                    // Operators
                    case '*': { Token.Type = TOKEN_Star; Token.Size = 1; } break;
                    case '%': { Token.Type = TOKEN_Modulo;         Token.Size = 1; } break;
                    case '+': { Token.Type = TOKEN_Addition;       Token.Size = 1; } break;
                    
                    case '<': { 
                        Token.Type = TOKEN_LessThan;
                        Token.Size = 1; 
                        if(Token.CppDirectiveHash == CppPredefHashes[CPPDIRECTIVE_include] || Token.CppDirectiveHash == CppPredefHashes[CPPDIRECTIVE_include_next]) {
                            u64 End = Token.Offset + 1;
                            for(; End < Buffer->Text.Used; End += utf8_char_width(&Buffer->Text.Data[End])) {
                                u8 Char = Buffer->Text.Data[End];
                                if(Char == '>') {
                                    End += 1;
                                    Token.Size = End - Token.Offset;
                                    Token.Type = TOKEN_IncludePath;
                                    break;
                                } else if(Char == '\n') {
                                    break;
                                }
                            }
                        }
                    } break;
                    
                    case '>': { Token.Type = TOKEN_GreaterThan; Token.Size = 1; } break;
                    case '-': { Token.Type = TOKEN_Subtraction; Token.Size = 1; } break;
                    case '&': { Token.Type = TOKEN_And;         Token.Size = 1; } break;
                    case '!': { Token.Type = TOKEN_LogicalNot;  Token.Size = 1; } break;
                    case '^': { Token.Type = TOKEN_Xor;         Token.Size = 1; } break;
                    case '~': { Token.Type = TOKEN_BitwiseNot;  Token.Size = 1; } break;
                    case '=': { Token.Type = TOKEN_Assignment;  Token.Size = 1; } break;
                    case ';': { Token.Type = TOKEN_Semicolon;   Token.Size = 1; } break;
                    case ',': { Token.Type = TOKEN_Comma;       Token.Size = 1; } break;
                    
                    default: {
                        Token.Type = TOKEN_Unknown;
                        Token.Size = utf8_char_width(Cursor);
                    } break;
                }
            }
        } 
    } 
    
    *Out = Token;
    
    return HasNext;
}

b8
is_token_keyword(token_t *Token, cpp_keyword_type_t Keyword) {
    if(Keyword >= CPP_PREDEF_MAX) { 
        return false;
    }
    
    return (Token->Type == TOKEN_Keyword && Token->Hash == CppPredefHashes[Keyword]);
}

typedef enum {
    DECL_Function = 1,
    DECL_FunctionPrototype,
    DECL_Typedef,
    DECL_Enum,
    DECL_Struct,
    DECL_Class,
    DECL_Array
} declaration_type_t;

typedef struct {
    s64 Curly, Paren, Square;
} scope_depth_t;

b8
scope_depth_equal(scope_depth_t s0, scope_depth_t s1) {
    return (s0.Curly == s1.Curly) && (s0.Paren == s1.Paren) && (s0.Square == s1.Square);
}

typedef struct {
    u64 Offset;
    u64 Size;
    u64 IdentifierOffset;
    u64 IdentifierSize;
    
    scope_depth_t ScopeDepth;
    
    declaration_type_t Type;
} declaration_t;

void
do_declaration_stuff(buffer_t *Buffer) {
    token_t *Tokens = 0;
    for(token_t Token = {0}; next_token(Buffer, Token, &Token); ) {
        sb_push(Tokens, Token);
    }
    
    s64 DeclTokenStart = -1;
    declaration_t Decl = {0};
    
    scope_depth_t ScopeDepth = {0};
    scope_depth_t ScopeDepthZero = {0};
    
    declaration_t *Declarations = 0;
    
    for(s64 TokenIndex = 0; TokenIndex < sb_count(Tokens); ++TokenIndex) {
        token_t Tok = Tokens[TokenIndex];
        if((Tok.Type == TOKEN_Keyword || Tok.Type == TOKEN_Identifier) && DeclTokenStart == -1 && scope_depth_equal(ScopeDepth, ScopeDepthZero)) {
            DeclTokenStart = TokenIndex;
            Decl.Offset = Tok.Offset;
            Decl.ScopeDepth = ScopeDepth;
        }
        
        switch(Tok.Type) {
            case TOKEN_Keyword: {
                if(TokenIndex == DeclTokenStart) {
                    if(Tok.Hash == CppPredefHashes[KEYWORD_struct]) {
                        Decl.Type = DECL_Struct;
                    } else if(Tok.Hash == CppPredefHashes[KEYWORD_enum]) {
                        Decl.Type = DECL_Enum;
                    } else if(Tok.Hash == CppPredefHashes[KEYWORD_class]) {
                        Decl.Type = DECL_Class;
                    } 
                }
            } break;
            
            case TOKEN_Assignment: {
                mem_zero_struct(&Decl);
                DeclTokenStart = -1;
            } break;
            
            case TOKEN_Identifier: {
                b8 ScopeDepthEqual = scope_depth_equal(Decl.ScopeDepth, ScopeDepth);
                if(DeclTokenStart == TokenIndex || !ScopeDepthEqual) {
                    continue;
                } else {
                    token_t *Prev = &Tokens[TokenIndex - 1];
                    if((Decl.Type == DECL_Struct || Decl.Type == DECL_Enum || Decl.Type == DECL_Class) &&
                       (is_token_keyword(Prev, KEYWORD_struct) || is_token_keyword(Prev, KEYWORD_enum) || is_token_keyword(Prev, KEYWORD_struct))) {
                        // Example: struct foo_t { int x; } funcy_stuff() { return (struct foo_t){123}; }
                        // This defines a struct type foo_t that can be used later, so we need to save it as a 
                        // declaration on it's own.
                        declaration_t d = Decl;
                        d.Size = Tok.Offset + Tok.Size - Decl.Offset;
                        d.IdentifierOffset = Tok.Offset;
                        d.IdentifierSize = Tok.Size;
                        sb_push(Declarations, d);
                    }
                    
                    if((TokenIndex + 1) < sb_count(Tokens)) {
                        token_t Next = Tokens[TokenIndex + 1];
                        if(Next.Type == TOKEN_Bracket && Next.Bracket == '(') {
                            Decl.Type = DECL_Function;
                            Decl.IdentifierOffset = Tok.Offset;
                            Decl.IdentifierSize = Tok.Size;
                        }
                    }
                }
            } break;
            
            case TOKEN_Bracket: {
                switch(Tok.Bracket) {
                    case '{': { 
                        if(Decl.Type == DECL_Function && scope_depth_equal(Decl.ScopeDepth, ScopeDepth)) {
                            // This is the start of a function function, let's submit the function declaration
                            Decl.Size = Tok.Offset - Decl.Offset;
                            
                            sb_push(Declarations, Decl);
                            mem_zero_struct(&Decl);
                            DeclTokenStart = -1;
                        }
                        
                        ScopeDepth.Curly += 1; 
                    } break;
                    case '}': { ScopeDepth.Curly -= 1; } break;
                    
                    case '(': { ScopeDepth.Paren += 1; } break;
                    case ')': { ScopeDepth.Paren -= 1; } break;
                    
                    case '[': { ScopeDepth.Square += 1; } break;
                    case ']': { ScopeDepth.Square -= 1; } break;
                }
            } break;
            
            case TOKEN_Semicolon: {
                b8 ScopeDepthEqual = scope_depth_equal(Decl.ScopeDepth, ScopeDepth);
                if(ScopeDepthEqual) {
                    if(Decl.Type == DECL_Function) {
                        // This is the start of a function function, let's submit the function declaration
                        Decl.Size = Tok.Offset - Decl.Offset;
                        Decl.Type = DECL_FunctionPrototype;
                        
                        sb_push(Declarations, Decl);
                        mem_zero_struct(&Decl);
                    }
                    DeclTokenStart = -1;
                }
            } break;
            
            default: {
                
            } break;
        }
    }
    
#if 0    
    for(s64 i = 0; i < sb_count(Declarations); ++i) {
        declaration_t d = Declarations[i];
        int a = 3;
    }
#endif
    
    sb_free(Tokens);
    sb_free(Declarations);
}