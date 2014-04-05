#include <iostream>
#include <sstream>
#include <memory>
#include "make_unique.hh"

#include "op.hh"
#include "dwit.hh"
#include "dwpp.hh"
#include "dwcst.hh"

valfile::uptr
op_origin::next ()
{
  return std::move (m_vf);
}

std::string
op_origin::name () const
{
  return "origin";
}

void
op_origin::set_next (valfile::uptr vf)
{
  assert (m_vf == nullptr);

  // set_next should have been preceded with a reset() call that
  // should have percolated all the way here.
  assert (m_reset);
  m_reset = false;

  m_vf = std::move (vf);
}

void
op_origin::reset ()
{
  m_vf = nullptr;
  m_reset = true;
}


struct op_sel_universe::pimpl
{
  std::shared_ptr <op> m_upstream;
  std::shared_ptr <Dwarf> m_dw;
  all_dies_iterator m_it;
  valfile::uptr m_vf;
  size_t m_size;
  slot_idx m_dst;

  pimpl (std::shared_ptr <op> upstream, dwgrep_graph::ptr q,
	 size_t size, slot_idx dst)
    : m_upstream (upstream)
    , m_dw (q->dwarf)
    , m_it (all_dies_iterator::end ())
    , m_size (size)
    , m_dst (dst)
  {}

  valfile::uptr
  next ()
  {
    while (true)
      {
	if (m_vf == nullptr)
	  {
	    m_vf = m_upstream->next ();
	    if (m_vf == nullptr)
	      return nullptr;
	    m_it = all_dies_iterator (&*m_dw);
	  }

	if (m_it != all_dies_iterator::end ())
	  {
	    auto ret = valfile::copy (*m_vf, m_size);
	    ret->set_slot (m_dst, **m_it);
	    ++m_it;
	    return ret;
	  }

	m_vf = nullptr;
      }
  }

  void
  reset ()
  {
    m_vf = nullptr;
    m_upstream->reset ();
  }
};

op_sel_universe::op_sel_universe (std::shared_ptr <op> upstream,
				  dwgrep_graph::ptr q,
				  size_t size, slot_idx dst)
  : m_pimpl (std::make_unique <pimpl> (upstream, q, size, dst))
{}

op_sel_universe::~op_sel_universe ()
{}

valfile::uptr
op_sel_universe::next ()
{
  return m_pimpl->next ();
}

std::string
op_sel_universe::name () const
{
  return "sel_universe";
}

void
op_sel_universe::reset ()
{
  m_pimpl->reset ();
}

struct op_f_child::pimpl
{
  std::shared_ptr <op> m_upstream;
  std::shared_ptr <Dwarf> m_dw;
  valfile::uptr m_vf;
  Dwarf_Die m_child;

  size_t m_size;
  slot_idx m_src;
  slot_idx m_dst;

  pimpl (std::shared_ptr <op> upstream,
	 dwgrep_graph::ptr q,
	 size_t size, slot_idx src, slot_idx dst)
    : m_upstream (upstream)
    , m_dw (q->dwarf)
    , m_child {}
    , m_size (size)
    , m_src (src)
    , m_dst (dst)
  {}

  valfile::uptr
  next ()
  {
    while (true)
      {
	while (m_vf == nullptr)
	  {
	    if (auto vf = m_upstream->next ())
	      {
		if (vf->get_slot_type (m_src) == slot_type::DIE)
		  {
		    Dwarf_Die die = vf->get_slot_die (m_src);
		    if (dwarf_haschildren (&die))
		      {
			if (dwarf_child (&die, &m_child) != 0)
			  throw_libdw ();

			// We found our guy.
			m_vf = std::move (vf);
		      }
		  }
	      }
	    else
	      return nullptr;
	  }

	auto ret = valfile::copy (*m_vf, m_size);
	if (m_src == m_dst)
	  ret->invalidate_slot (m_dst);
	ret->set_slot (m_dst, m_child);

	switch (dwarf_siblingof (&m_child, &m_child))
	  {
	  case -1:
	    throw_libdw ();
	  case 1:
	    // No more siblings.
	    m_vf = nullptr;
	    break;
	  case 0:
	    break;
	  }

	return ret;
      }
  }

  void
  reset ()
  {
    m_vf = nullptr;
    m_upstream->reset ();
  }
};

op_f_child::op_f_child (std::shared_ptr <op> upstream,
			dwgrep_graph::ptr q,
			size_t size, slot_idx src, slot_idx dst)
  : m_pimpl (std::make_unique <pimpl> (upstream, q, size, src, dst))
{}

op_f_child::~op_f_child ()
{}

valfile::uptr
op_f_child::next ()
{
  return m_pimpl->next ();
}

std::string
op_f_child::name () const
{
  return "op_f_child";
}

void
op_f_child::reset ()
{
  m_pimpl->reset ();
}


valfile::uptr
op_nop::next ()
{
  return m_upstream->next ();
}

std::string
op_nop::name () const
{
  return "nop";
}


valfile::uptr
op_assert::next ()
{
  while (auto vf = m_upstream->next ())
    if (m_pred->result (*vf) == pred_result::yes)
      return vf;
  return nullptr;
}

std::string
op_assert::name () const
{
  std::stringstream ss;
  ss << "assert<" << m_pred->name () << ">";
  return ss.str ();
}

valfile::uptr
dieop_f::next ()
{
  while (auto vf = m_upstream->next ())
    if (vf->get_slot_type (m_src) == slot_type::DIE)
      {
	Dwarf_Die die = vf->get_slot_die (m_src);
	if (m_src == m_dst)
	  vf->invalidate_slot (m_dst);
	if (operate (*vf, m_dst, die))
	  return vf;
      }
  return nullptr;
}

bool
op_f_atval::operate (valfile &vf, slot_idx dst, Dwarf_Die die) const
{
  Dwarf_Attribute attr;

  if (dwarf_attr_integrate (&die, m_name, &attr) == nullptr)
    return false;

  switch (dwarf_whatform (&attr))
    {
    case DW_FORM_string:
    case DW_FORM_strp:
    case DW_FORM_GNU_strp_alt:
      {
	const char *str = dwarf_formstring (&attr);
	if (str == nullptr)
	  throw_libdw ();
	vf.set_slot (dst, std::string (str));
	return true;
      }

    case DW_FORM_sdata:
      {
      data_signed:
	Dwarf_Sword sval;
	if (dwarf_formsdata (&attr, &sval) != 0)
	  throw_libdw ();
	constant cst { (uint64_t) sval, &signed_constant_dom };
	vf.set_slot (dst, std::move (cst));
	return true;
      }

    case DW_FORM_udata:
      {
      data_unsigned:
	Dwarf_Word uval;
	if (dwarf_formudata (&attr, &uval) != 0)
	  throw_libdw ();
	constant cst { uval, &unsigned_constant_dom };
	vf.set_slot (dst, std::move (cst));
	return true;
      }

    case DW_FORM_data1:
    case DW_FORM_data2:
    case DW_FORM_data4:
    case DW_FORM_data8:
      switch (m_name)
	{
	case DW_AT_byte_stride:
	case DW_AT_bit_stride:
	  // """Note that the stride can be negative."""
	case DW_AT_binary_scale:
	case DW_AT_decimal_scale:
	  goto data_signed;

	case DW_AT_byte_size:
	case DW_AT_bit_size:
	case DW_AT_bit_offset:
	case DW_AT_data_bit_offset:
	case DW_AT_lower_bound:
	case DW_AT_upper_bound:
	case DW_AT_count:
	case DW_AT_allocated:
	case DW_AT_associated:
	case DW_AT_start_scope:
	case DW_AT_digit_count:
	case DW_AT_GNU_odr_signature:
	  goto data_unsigned;

	case DW_AT_ordering:
	case DW_AT_language:
	case DW_AT_visibility:
	case DW_AT_inline:
	case DW_AT_accessibility:
	case DW_AT_address_class:
	case DW_AT_calling_convention:
	case DW_AT_encoding:
	case DW_AT_identifier_case:
	case DW_AT_virtuality:
	case DW_AT_endianity:
	case DW_AT_decimal_sign:
	case DW_AT_decl_line:
	case DW_AT_call_line:
	case DW_AT_decl_column:
	case DW_AT_call_column:
	  // XXX these are Dwarf constants and should have
	  // a domain associated to them.
	  goto data_unsigned;

	case DW_AT_discr_value:
	case DW_AT_const_value:
	  // """The number is signed if the tag type for
	  // the variant part containing this variant is a
	  // signed type."""
	  assert (! "signedness of attribute not implemented yet");
	  abort ();
	}
      assert (! "signedness of attribute unhandled");
      abort ();

    case DW_FORM_addr:
    case DW_FORM_block2:
    case DW_FORM_block4:
    case DW_FORM_block:
    case DW_FORM_block1:
    case DW_FORM_flag:
    case DW_FORM_ref_addr:
    case DW_FORM_ref1:
    case DW_FORM_ref2:
    case DW_FORM_ref4:
    case DW_FORM_ref8:
    case DW_FORM_ref_udata:
    case DW_FORM_indirect:
    case DW_FORM_sec_offset:
    case DW_FORM_exprloc:
    case DW_FORM_flag_present:
    case DW_FORM_ref_sig8:
    case DW_FORM_GNU_ref_alt:
      assert (! "Form unhandled.");
      abort  ();
    }

  assert (! "Unhandled DWARF form type.");
  abort ();
}

std::string
op_f_atval::name () const
{
  std::stringstream ss;
  ss << "f_atval<" << m_name << ">";
  return ss.str ();
}

bool
op_f_offset::operate (valfile &vf, slot_idx dst, Dwarf_Die die) const
{
  Dwarf_Off off = dwarf_dieoffset (&die);
  vf.set_slot (dst, constant { off, &unsigned_constant_dom });
  return true;
}

std::string
op_f_offset::name () const
{
  return "f_offset";
}

/*
std::unique_ptr <yielder>
op_format::get_yielder (std::unique_ptr <stack> stk, size_t stksz,
			std::shared_ptr <Dwarf> &dw) const
{
  class format_yielder
    : public yielder
  {
    bool done;
    std::shared_ptr <Dwarf> &m_dw;
    std::string const &m_lit;
    size_t m_idx;

  public:
    format_yielder (std::shared_ptr <Dwarf> &dw,
		    std::string const &lit, size_t idx)
      : done (false)
      , m_dw (dw)
      , m_lit (lit)
      , m_idx (idx)
    {}

    virtual std::unique_ptr <stack>
    yield () override
    {
      if (done)
	return nullptr;

      done = true;
      auto ret = stk.clone (stksz);
      auto nv = std::make_unique <v_str> (m_lit.c_str ());
      ret->set_slot (m_idx, std::move (nv));
      return ret;
    }
  };

  return std::make_unique <format_yielder> (dw, m_str, m_idx);
}

std::unique_ptr <yielder>
op_drop::get_yielder (std::unique_ptr <stack> stk, size_t stksz,
		      std::shared_ptr <Dwarf> &dw) const
{
  class drop_yielder
    : public yielder
  {
    bool done;
    size_t m_idx;

  public:
    explicit drop_yielder (size_t idx)
      : done (false)
      , m_idx (idx)
    {}

    virtual std::unique_ptr <stack>
    yield () override
    {
      if (done)
	return nullptr;

      done = true;
      auto ret = stk.clone (stksz);
      ret->invalidate_slot (m_idx);
      return ret;
    }
  };

  return std::make_unique <drop_yielder> (m_idx);
}
*/

valfile::uptr
op_const::next ()
{
  while (auto vf = m_upstream->next ())
    {
      vf->set_slot (m_dst, constant (m_cst));
      return vf;
    }
  return nullptr;
}

std::string
op_const::name () const
{
  std::stringstream ss;
  ss << "const<" << m_cst << ">";
  return ss.str ();
}


pred_result
pred_not::result (valfile &vf)
{
  return ! m_a->result (vf);
}

std::string
pred_not::name () const
{
  std::stringstream ss;
  ss << "not <" << m_a->name () << ">";
  return ss.str ();
}


pred_result
pred_and::result (valfile &vf)
{
  return m_a->result (vf) && m_b->result (vf);
}

std::string
pred_and::name () const
{
  std::stringstream ss;
  ss << "and <" << m_a->name () << "><" << m_b->name () << ">";
  return ss.str ();
}


pred_result
pred_or::result (valfile &vf)
{
  return m_a->result (vf) || m_b->result (vf);
}

std::string
pred_or::name () const
{
  std::stringstream ss;
  ss << "or <" << m_a->name () << "><" << m_b->name () << ">";
  return ss.str ();
}

pred_result
pred_at::result (valfile &vf)
{
  if (vf.get_slot_type (m_idx) == slot_type::DIE)
    {
      auto &die = const_cast <Dwarf_Die &> (vf.get_slot_die (m_idx));
      return pred_result (dwarf_hasattr_integrate (&die, m_atname) != 0);
    }
  else if (vf.get_slot_type (m_idx) == slot_type::ATTRIBUTE)
    {
      auto &attr = const_cast <Dwarf_Attribute &> (vf.get_slot_attr (m_idx));
      return pred_result (dwarf_whatattr (&attr) == m_atname);
    }
  else
    return pred_result::fail;
}

std::string
pred_at::name () const
{
  std::stringstream ss;
  ss << "pred_at<" << m_atname << ">";
  return ss.str ();
}

pred_result
pred_tag::result (valfile &vf)
{
  if (vf.get_slot_type (m_idx) == slot_type::DIE)
    {
      auto &die = const_cast <Dwarf_Die &> (vf.get_slot_die (m_idx));
      return pred_result (dwarf_tag (&die) == m_tag);
    }
  else
    return pred_result::fail;
}

std::string
pred_tag::name () const
{
  std::stringstream ss;
  ss << "pred_tag<" << m_tag << ">";
  return ss.str ();
}

namespace
{
  template <class CMP>
  pred_result
  comparison_result (valfile &vf, slot_idx idx_a, slot_idx idx_b)
  {
    slot_type st = vf.get_slot_type (idx_a);
    if (st != vf.get_slot_type (idx_b))
      return pred_result::fail;

    switch (st)
      {
      case slot_type::END:
      case slot_type::INVALID:
	assert (! "invalid slots for comparison");
	abort ();

      case slot_type::CST:
	return CMP::result (vf.get_slot_cst (idx_a), vf.get_slot_cst (idx_b));

      case slot_type::STR:
	return CMP::result (vf.get_slot_str (idx_a), vf.get_slot_str (idx_b));

      case slot_type::FLT:
      case slot_type::SEQ:
      case slot_type::DIE:
      case slot_type::ATTRIBUTE:
      case slot_type::LINE:
      case slot_type::LOCLIST_ENTRY:
      case slot_type::LOCLIST_OP:
	assert (! "NYI");
	abort ();
      }

    assert (! "invalid slot type");
    abort ();
  }

  struct cmp_eq
  {
    template <class T>
    static pred_result
    result (T const &a, T const &b)
    {
      return pred_result (a == b);
    }
  };

  struct cmp_lt
  {
    template <class T>
    static pred_result
    result (T const &a, T const &b)
    {
      return pred_result (a < b);
    }
  };

  struct cmp_gt
  {
    template <class T>
    static pred_result
    result (T const &a, T const &b)
    {
      return pred_result (a > b);
    }
  };
}

pred_result
pred_eq::result (valfile &vf)
{
  return comparison_result <cmp_eq> (vf, m_idx_a, m_idx_b);
}

std::string
pred_eq::name () const
{
  return "pred_eq";
}

pred_result
pred_lt::result (valfile &vf)
{
  return comparison_result <cmp_lt> (vf, m_idx_a, m_idx_b);
}

std::string
pred_lt::name () const
{
  return "pred_lt";
}

pred_result
pred_gt::result (valfile &vf)
{
  return comparison_result <cmp_gt> (vf, m_idx_a, m_idx_b);
}

std::string
pred_gt::name () const
{
  return "pred_gt";
}

pred_result
pred_root::result (valfile &vf)
{
  // this is very slow.  We should use our parent/prev cache for this.
  // NIY.
  if (vf.get_slot_type (m_idx_a) == slot_type::DIE)
    {
      auto &die = const_cast <Dwarf_Die &> (vf.get_slot_die (m_idx_a));
      for (auto it = cu_iterator { &*m_q->dwarf };
	   it != cu_iterator::end (); ++it)
	{
	  Dwarf_Die *cudiep = *it;
	  if (dwarf_dieoffset (&die) == dwarf_dieoffset (cudiep))
	    return pred_result::yes;
	}
      return pred_result::no;
    }
  else
    return pred_result::fail;
}

std::string
pred_root::name () const
{
  return "pred_root";
}

pred_result
pred_subx_any::result (valfile &vf)
{
  m_op->reset ();
  m_origin->set_next (valfile::copy (vf, m_size));
  if (m_op->next () != nullptr)
    return pred_result::yes;
  else
    return pred_result::no;
}

std::string
pred_subx_any::name () const
{
  std::stringstream ss;
  ss << "pred_subx_any<" << m_op->name () << ">";
  return ss.str ();
}

void
pred_subx_any::reset ()
{
  m_op->reset ();
}
