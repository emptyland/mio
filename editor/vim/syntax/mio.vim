
syntax keyword mioContant true false inf32 inf64 NaN32 NaN64
syntax keyword mioKeyword and or not package with as is bool i8 i16 i32 i64 int
syntax keyword mioKeyword f32 f64 string error void union map slice array struct
syntax keyword mioKeyword external weak strong if else while for match in return
syntax keyword mioKeyword break continue val var function lambda def native
syntax keyword mioKeyword export len add delete typeof

syntax keyword mioOperator len map filter add delete

syntax keyword mioFunc eof gc fullGC tick allGlobalVariables println print
syntax keyword mioFunc panic newError newErrorWith sleep

syntax match mioInteger "\<\d\+\>"
syntax match mioInteger "\<\-\d\+\>"
syntax match mioInteger "\<\+\d\+\>"
syntax match mioFloat "\<\d\*\.\d\+\>"
syntax match mioHex "\<0x\x\+\>"
syntax match mioFix "FIX\|FIXME\|TODO\|XXX"

syntax region mioComment start="//" skip="\\$" end="$"
syntax region mioComment start="#" skip="\\$" end="$"
syntax region mioString start=+L\="+ skip=+\\\\\|\\"+ end=+"+
syntax region mioRawString start=+L\='+ skip=+\\\\\|\\'+ end=+'+

hi def link mioFunc      Identifier
hi def link mioKeyword   Type

hi mioFix ctermfg=6 cterm=bold guifg=#0000FF
hi mioOperator ctermfg=1
hi mioInteger ctermfg=5
hi mioFloat ctermfg=5
hi mioHex ctermfg=5
hi mioString ctermfg=5
hi mioRawString ctermfg=1
hi mioComment ctermfg=2
hi mioContant ctermfg=5 guifg=#FFFF00
