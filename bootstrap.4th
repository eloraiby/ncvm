// bootstrap.4th

: +  u32.add ;
: -  u32.sub ;
: *  u32.mul ;
: /  u32.div ;
: %  u32.mod ;
: &  u32.and ;
: |  u32.or  ;
: ^  u32.xor ;
: ~  u32.not ;
: >> u32.shr ;
: << u32.shl ;
: =  u32.eq  ;
: != u32.neq ;
: >= u32.geq ;
; <= u32.leq ;
: >  u32.gt  ;
: <  u32.lt  ;
: ?  cond    ;
: ## call    ;

: test+ 1 2 + .i ;
: test* 2 3 * .i ;

: test-lambda-false 0 { 11 } { 22 } ? .i ;
: test-lambda-true 1 { 11 } { 22 } ? .i ;


