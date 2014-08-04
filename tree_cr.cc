/*
   Copyright (C) 2014 Red Hat, Inc.
   This file is part of dwgrep.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   dwgrep is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include "scope.hh"
#include "tree_cr.hh"

tree *
tree::create_builtin (std::shared_ptr <builtin const> b)
{
  auto t = new tree {tree_type::F_BUILTIN};
  t->m_builtin = b;
  return t;
}

tree *
tree::create_neg (tree *t)
{
  return tree::create_unary <tree_type::PRED_NOT> (t);
}

tree *
tree::create_assert (tree *t)
{
  return tree::create_unary <tree_type::ASSERT> (t);
}

void
tree::take_child (tree *t)
{
  m_children.push_back (*t);
  delete t;
}

void
tree::take_child_front (tree *t)
{
  m_children.insert (m_children.begin (), *t);
  delete t;
}

void
tree::take_cat (tree *t)
{
  m_children.insert (m_children.end (),
		     t->m_children.begin (), t->m_children.end ());
}


namespace
{
  tree
  promote_scopes (tree t, std::shared_ptr <scope> scp)
  {
    switch (t.tt ())
      {
      case tree_type::ALT:
      case tree_type::CAPTURE:
      case tree_type::OR:
      case tree_type::SCOPE:
      case tree_type::BLOCK:
      case tree_type::CLOSE_STAR:
      case tree_type::IFELSE:
      case tree_type::PRED_SUBX_ANY:
	for (auto &c: t.m_children)
	  c = tree::promote_scopes (c, scp);
	return t;

      case tree_type::BIND:
	if (scp->has_name (t.str ()))
	  throw std::runtime_error (std::string {"Name `"}
				    + t.str () + "' rebound.");
	scp->add_name (t.str ());
	assert (t.m_children.size () == 0);
	return t;

      case tree_type::ASSERT:
      case tree_type::CAT: case tree_type::READ: case tree_type::EMPTY_LIST:
      case tree_type::PRED_NOT:
      case tree_type::PRED_AND: case tree_type::NOP: case tree_type::PRED_OR:
      case tree_type::CONST: case tree_type::STR: case tree_type::FORMAT:
      case tree_type::F_BUILTIN: case tree_type::F_DEBUG:
	for (auto &c: t.m_children)
	  c = ::promote_scopes (c, scp);
	return t;
      }
    return t;
  }

  tree
  build_scope (tree t, std::shared_ptr <scope> scope)
  {
    if (scope->empty ())
      return t;
    else
      {
	tree ret {tree_type::SCOPE, scope};
	ret.m_children.push_back (t);
	return ret;
      }
  }
}

tree
tree::promote_scopes (tree t, std::shared_ptr <scope> parent)
{
  auto scp = std::make_shared <scope> (parent);
  auto t2 = ::promote_scopes (t, scp);
  return build_scope (t2, scp);
}
