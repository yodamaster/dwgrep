#ifndef _TREE_H_
#define _TREE_H_

#include <vector>
#include <string>

#include "constant.hh"
#include "op.hh"
#include "valfile.hh"

// These constants describe how a tree is allowed to be constructed.
// It's mostly present to make sure we don't inadvertently construct
// e.g. a CONST tree with no associated constant.
//
// NULLARY, UNARY, BINARY -- a tree that's constructed over zero, one
// or two sub-trees.  Often that's the only number of sub-trees that
// makes sense for given tree type, but CAT and ALT can hold arbitrary
// number of sub-trees, they are just constructed from two.  FORMAT is
// NULLARY, but holds any number of sub-trees as well.
//
// STR, CONST -- A tree that holds a constant, has no children.
enum class tree_arity_v
  {
    NULLARY,
    UNARY,
    BINARY,
    STR,
    CST,
  };

// CAT -- A node for holding concatenation (X Y Z).
// ALT -- A node for holding alternation (X, Y, Z).
// CAPTURE -- For holding [X].
//
// TRANSFORM -- For holding NUM/X.  The first child is a constant
// representing application depth, the second is the X.
//
// PROTECT -- For holding +X.
//
// NOP -- For holding a no-op that comes up in "%s" and (,X).
//
// CLOSE_STAR, CLOSE_PLUS, MAYBE -- For holding X*, X+, X?.  X is the
// only child of these nodes.
//
// ASSERT -- All assertions (such as ?some_tag) are modeled using an
// ASSERT node, whose only child is a predicate node expressing the
// asserted condition (such as PRED_TAG).  Negative assertions are
// modeled using PRED_NOT.  In particular, IF is expanded into (!empty
// drop), ELSE into (?empty drop).
//
// PRED_SUBX_ALL, PRED_SUBX_ANY -- For holding ?all{X} and ?{X}.
// !all{X} and !{} are modeled using PRED_NOT.
//
// CONST -- For holding named constants and integer literals.
//
// STR -- For holding string literals.  Actual string literals are
// translated to FORMAT.
//
// FORMAT -- The literal parts of the format string are stored as
// children of type STR.  Other computation is stored as children of
// other types.
//
// F_ADD, F_SUB, etc. -- For holding corresponding function words.
//
// F_CAST -- For domain casting.  The argument is a constant, whose
// domain determines what domain to cast to.
//
// SEL_UNIVERSE, SEL_SECTION, SEL_UNIT -- Likewise.  But note that
// SEL_UNIVERSE does NOT actually POP.  Plain "universe" is translated
// as "SHF_DROP SEL_UNIVERSE", and "+universe" is translated as mere
// "SEL_UNIVERSE".
//
// SHF_SWAP, SWP_DUP -- Stack shuffling words.

#define TREE_TYPES				\
  TREE_TYPE (CAT, BINARY)			\
  TREE_TYPE (ALT, BINARY)			\
  TREE_TYPE (CAPTURE, UNARY)			\
  TREE_TYPE (EMPTY_LIST, NULLARY)		\
  TREE_TYPE (TRANSFORM, BINARY)			\
  TREE_TYPE (PROTECT, UNARY)			\
  TREE_TYPE (NOP, NULLARY)			\
  TREE_TYPE (CLOSE_PLUS, UNARY)			\
  TREE_TYPE (CLOSE_STAR, UNARY)			\
  TREE_TYPE (MAYBE, UNARY)			\
  TREE_TYPE (ASSERT, UNARY)			\
  TREE_TYPE (PRED_AT, CST)			\
  TREE_TYPE (PRED_TAG, CST)			\
  TREE_TYPE (PRED_EQ, NULLARY)			\
  TREE_TYPE (PRED_NE, NULLARY)			\
  TREE_TYPE (PRED_GT, NULLARY)			\
  TREE_TYPE (PRED_GE, NULLARY)			\
  TREE_TYPE (PRED_LT, NULLARY)			\
  TREE_TYPE (PRED_LE, NULLARY)			\
  TREE_TYPE (PRED_FIND, NULLARY)		\
  TREE_TYPE (PRED_MATCH, NULLARY)		\
  TREE_TYPE (PRED_EMPTY, NULLARY)		\
  TREE_TYPE (PRED_ROOT, NULLARY)		\
  TREE_TYPE (PRED_AND, BINARY)			\
  TREE_TYPE (PRED_OR, BINARY)			\
  TREE_TYPE (PRED_NOT, UNARY)			\
  TREE_TYPE (PRED_SUBX_ALL, UNARY)		\
  TREE_TYPE (PRED_SUBX_ANY, UNARY)		\
  TREE_TYPE (PRED_LAST, NULLARY)		\
  TREE_TYPE (CONST, CST)			\
  TREE_TYPE (STR, STR)				\
  TREE_TYPE (FORMAT, NULLARY)			\
  TREE_TYPE (F_ADD, NULLARY)			\
  TREE_TYPE (F_SUB, NULLARY)			\
  TREE_TYPE (F_MUL, NULLARY)			\
  TREE_TYPE (F_DIV, NULLARY)			\
  TREE_TYPE (F_MOD, NULLARY)			\
  TREE_TYPE (F_PARENT, NULLARY)			\
  TREE_TYPE (F_CHILD, NULLARY)			\
  TREE_TYPE (F_ATTRIBUTE, NULLARY)		\
  TREE_TYPE (F_ATTR_NAMED, CST)			\
  TREE_TYPE (F_PREV, NULLARY)			\
  TREE_TYPE (F_NEXT, NULLARY)			\
  TREE_TYPE (F_TYPE, NULLARY)			\
  TREE_TYPE (F_OFFSET, NULLARY)			\
  TREE_TYPE (F_NAME, NULLARY)			\
  TREE_TYPE (F_TAG, NULLARY)			\
  TREE_TYPE (F_FORM, NULLARY)			\
  TREE_TYPE (F_VALUE, NULLARY)			\
  TREE_TYPE (F_POS, NULLARY)			\
  TREE_TYPE (F_COUNT, NULLARY)			\
  TREE_TYPE (F_EACH, NULLARY)			\
  TREE_TYPE (F_LENGTH, NULLARY)			\
  TREE_TYPE (F_CAST, CST)			\
  TREE_TYPE (SEL_UNIVERSE, NULLARY)		\
  TREE_TYPE (SEL_SECTION, NULLARY)		\
  TREE_TYPE (SEL_UNIT, NULLARY)			\
  TREE_TYPE (SHF_SWAP, NULLARY)			\
  TREE_TYPE (SHF_DUP, NULLARY)			\
  TREE_TYPE (SHF_OVER, NULLARY)			\
  TREE_TYPE (SHF_ROT, NULLARY)			\
  TREE_TYPE (SHF_DROP, NULLARY)			\

enum class tree_type
  {
#define TREE_TYPE(ENUM, ARITY) ENUM,
    TREE_TYPES
#undef TREE_TYPE
  };

template <tree_type TT> class tree_arity;
#define TREE_TYPE(ENUM, ARITY)					\
  template <> struct tree_arity <tree_type::ENUM> {		\
    static const tree_arity_v value = tree_arity_v::ARITY;	\
  };
TREE_TYPES
#undef TREE_TYPE

// This is for communication between lexical and syntactic analyzers
// and the rest of the world.  It uses naked pointers all over the
// place, as in the %union that the lexer and parser use, we can't
// hold smart pointers.
class tree
{
  std::unique_ptr <std::string> m_str;
  std::unique_ptr <constant> m_cst;

public:
  tree_type m_tt;
  std::vector <tree> m_children;

  // -1 if not initialized, otherwise a slot number.
  ssize_t m_src_a;
  ssize_t m_src_b;
  ssize_t m_dst;

  tree ();
  explicit tree (tree_type tt);
  tree (tree const &other);

  tree (tree_type tt, std::string const &str);
  tree (tree_type tt, constant const &cst);

  tree &operator= (tree other);
  void swap (tree &other);

  slot_idx src_a () const;
  slot_idx src_b () const;
  slot_idx dst () const;

  tree &child (size_t idx);
  tree const &child (size_t idx) const;

  std::string &str () const;
  constant &cst () const;

  void push_child (tree const &t);
  void dump (std::ostream &o) const;

  // === Build interface ===
  //
  // The following methods are implemented in build.cc.  They are for
  // translation of trees to actual execution nodes.

  // Remove unnecessary operations--some stack shuffling can be
  // eliminated and protect nodes.
  // XXX this should actually be hidden behind build_exec or what not.
  void simplify ();

  // This initializes m_src_a and/or m_src_b and/or m_dst of each
  // operation.  It returns the maximum stack size necessary for the
  // computation.  As it works through the AST, it checks a number of
  // invariants, such as number of children, stack underruns, etc.
  // XXX this should actually be hidden behind build_exec or what not.
  size_t determine_stack_effects ();

  // This should build an op node corresponding to this expression.
  //
  // Not every expression node needs to have an associated op, some
  // nodes will only install some plumbing (in particular, a CAT node
  // would only create a series of nested op's).  UPSTREAM should be
  // nullptr if this is the toplevel-most expression, otherwise it
  // should be a valid op that the op produced by this node feeds off.
  //
  // XXX Note that the MAXSIZE argument should go away.  This needs to
  // be set for each producer node by determine_stack_effects, and
  // should capture stack needs of the following computations.  NIY.
  std::shared_ptr <op>
  build_exec (std::shared_ptr <op> upstream,
	      dwgrep_graph::sptr q, size_t maxsize) const;

  // Produce program suitable for interpretation.
  //
  // XXX see above for maxsize.  This should go away.  NIY.
  std::unique_ptr <pred> build_pred (dwgrep_graph::sptr q,
				     size_t maxsize) const;

  // === Parser interface ===
  //
  // The following methods are implemented in tree_cr.hh and
  // tree_cr.cc.  They are meant as parser interface, otherwise value
  // semantics should be preferred.

  template <tree_type TT> static tree *create_nullary ();
  template <tree_type TT> static tree *create_unary (tree *op);
  template <tree_type TT> static tree *create_binary (tree *lhs, tree *rhs);
  template <tree_type TT> static tree *create_str (std::string s);
  template <tree_type TT> static tree *create_const (constant c);
  template <tree_type TT> static tree *create_cat (tree *t1, tree *t2);

  static tree *create_neg (tree *t1);
  static tree *create_assert (tree *t1);
  static tree *create_protect (tree *t1);

  // push_back (*T) and delete T.
  void take_child (tree *t);

  // push_front (*T) and delete T.
  void take_child_front (tree *t);

  // Takes a tree T, which is a CAT or an ALT, and appends all
  // children therein.  It then deletes T.
  void take_cat (tree *t);
};

#endif /* _TREE_H_ */
