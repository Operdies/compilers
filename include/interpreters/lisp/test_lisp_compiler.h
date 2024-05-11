#pragma once

struct lispObject {
  int type;
};

struct lispObject lisp_eval(const char *expression);
