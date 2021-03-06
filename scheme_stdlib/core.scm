(define map
  (lambda (f l)
    (if (null? l)
        '()
        (cons (f (car l)) (map f (cdr l))))))

(define accumulate
  (lambda (f init l)
    (if (null? l)
        init
        (f (car l)
           (accumulate f init (cdr l))))))

(define filter
  (lambda (f l)
    (if (null? l)
        '()
        (if (f (car l))
            (cons (car l) (filter f (cdr l)))
            (filter f (cdr l))))))

(define id
  (lambda (x) x))

(define length
  (lambda (l)
    (cond ((null? l) 0)
          (else (+ 1 (length (cdr l)))))))

(define append-two-lists
  (lambda (ss ts)
    (cond ((null? ss) ts)
          (else
           (cons (car ss)
                 (append-two-lists (cdr ss) ts))))))

(define curry
  (lambda (f arg1)
    (lambda (#!rest args)
      (apply f (cons arg1 args)))))

(define caar
  (lambda (x) (car (car x))))
(define cadr
  (lambda (x) (car (cdr x))))
(define cdar
  (lambda (x) (cdr (car x))))
(define cddr
  (lambda (x) (cdr (cdr x))))
(define caaar
  (lambda (x) (car (car (car x)))))
(define caadr
  (lambda (x) (car (car (cdr x)))))
(define cadar
  (lambda (x) (car (cdr (car x)))))
(define caddr
  (lambda (x) (car (cdr (cdr x)))))
(define cdaar
  (lambda (x) (cdr (car (car x)))))
(define cdadr
  (lambda (x) (cdr (car (cdr x)))))
(define cddar
  (lambda (x) (cdr (cdr (car x)))))
(define cdddr
  (lambda (x) (cdr (cdr (cdr x)))))
(define caaaar
  (lambda (x) (car (car (car (car x))))))
(define caaadr
  (lambda (x) (car (car (car (cdr x))))))
(define caadar
  (lambda (x) (car (car (cdr (car x))))))
(define caaddr
  (lambda (x) (car (car (cdr (cdr x))))))
(define cadaar
  (lambda (x) (car (cdr (car (car x))))))
(define cadadr
  (lambda (x) (car (cdr (car (cdr x))))))
(define caddar
  (lambda (x) (car (cdr (cdr (car x))))))
(define cadddr
  (lambda (x) (car (cdr (cdr (cdr x))))))
(define cdaaar
  (lambda (x) (cdr (car (car (car x))))))
(define cdaadr
  (lambda (x) (cdr (car (car (cdr x))))))
(define cdadar
  (lambda (x) (cdr (car (cdr (car x))))))
(define cdaddr
  (lambda (x) (cdr (car (cdr (cdr x))))))
(define cddaar
  (lambda (x) (cdr (cdr (car (car x))))))
(define cddadr
  (lambda (x) (cdr (cdr (car (cdr x))))))
(define cdddar
  (lambda (x) (cdr (cdr (cdr (car x))))))
(define cddddr
  (lambda (x) (cdr (cdr (cdr (cdr x))))))
