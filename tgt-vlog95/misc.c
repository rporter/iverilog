/*
 * Copyright (C) 2011 Cary R. (cygcary@yahoo.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

# include <stdlib.h>
# include <string.h>
# include "config.h"
# include "vlog95_priv.h"
# include "ivl_alloc.h"

/*
 * Emit a constant delay that has been rescaled to the given scopes timescale.
 */
void emit_scaled_delay(ivl_scope_t scope, uint64_t delay)
{
      int scale = ivl_scope_time_units(scope) - sim_precision;
      int pre = ivl_scope_time_units(scope) - ivl_scope_time_precision(scope);
      char *frac;
      unsigned real_dly = 0;
      assert(scale >= 0);
      assert(pre >= 0);
      assert(scale >= pre);
      frac = (char *)malloc(pre+1);
      frac[pre] = 0;
      for (/* none */; scale > 0; scale -= 1) {
	    if (scale > pre) {
		  assert((delay % 10) == 0);
	    } else {
		  frac[scale-1] = (delay % 10) + '0';
		  if (frac[scale-1] != '0') {
			real_dly = 1;
		  } else if (!real_dly) {
			frac[scale-1] = 0;
		  }
	    }
	    delay /= 10;
      }
// HERE: If there is no frac then this has to fit into 31 bits like any
//       other integer.
      fprintf(vlog_out, "%"PRIu64, delay);
      if (real_dly) {
	    fprintf(vlog_out, ".%s", frac);
      }
      free(frac);
}

/*
 * Emit a constant or variable delay that has been rescaled to the given
 * scopes timescale.
 */
void emit_scaled_delayx(ivl_scope_t scope, ivl_expr_t expr)
{
      assert(! ivl_expr_signed(expr));
      if (ivl_expr_type(expr) == IVL_EX_NUMBER) {
	    int rtype;
	    uint64_t value = get_uint64_from_number(expr, &rtype);
	    if (rtype > 0) {
		  fprintf(vlog_out, "<invalid>");
		  fprintf(stderr, "%s:%u: vlog95 error: Time value is "
		                  "greater than 64 bits (%u) and cannot be "
		                  "safely represented.\n",
		                  ivl_expr_file(expr), ivl_expr_lineno(expr),
		                  rtype);
		  vlog_errors += 1;
		  return;
	    }
	    if (rtype < 0) {
		  fprintf(vlog_out, "<invalid>");
		  fprintf(stderr, "%s:%u: vlog95 error: Time value has an "
		                  "undefined bit and cannot be represented.\n",
		                  ivl_expr_file(expr), ivl_expr_lineno(expr));
		  vlog_errors += 1;
		  return;
	    }
	    emit_scaled_delay(scope, value);
      } else {
	    int exp = ivl_scope_time_units(scope) - sim_precision;
	    uint64_t scale = 1;
	    assert(exp >= 0);
	    if (exp == 0) emit_expr(scope, expr, 0);
	    else {
		  uint64_t scale_val;
		  int rtype;
		    /* This is as easy as removing the multiple that was
		     * added to scale the value to the simulation time,
		     * but we need to verify that the scaling value is
		     * correct first. */
		  if ((ivl_expr_type(expr) != IVL_EX_BINARY) ||
		      (ivl_expr_opcode(expr) != '*') ||
		      (ivl_expr_type(ivl_expr_oper2(expr)) != IVL_EX_NUMBER)) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Variable time "
			                "expression/value cannot be scaled.\n ",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr));
			vlog_errors += 1;
			return;
		  }
		  scale_val = get_uint64_from_number(ivl_expr_oper2(expr),
		                                     &rtype);
		  if (rtype > 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Variable time "
			                "expression/value scale coefficient "
			                "was greater then 64 bits (%d).\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr),
			                rtype);
			vlog_errors += 1;
			return;
		  }
		  if (rtype < 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Variable time "
			                "expression/value scale coefficient "
			                "has an undefined bit.\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr));
			vlog_errors += 1;
			return;
		  }
		  while (exp > 0) {
			scale *= 10;
			exp -= 1;
		  }
		  if (scale !=  scale_val) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Variable time "
			                "expression/value scale coefficient "
			                "did not match expected value "
			                "(%"PRIu64" != %"PRIu64").\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr),
			                scale, scale_val);
			vlog_errors += 1;
			return;
		  }
		  emit_expr(scope, ivl_expr_oper1(expr), 0);
	    }
      }
}

void emit_scaled_range(ivl_scope_t scope, ivl_expr_t expr, unsigned width,
                       int msb, int lsb)
{
      if (msb >= lsb) {
	    if (ivl_expr_type(expr) == IVL_EX_NUMBER) {
		  int rtype;
		  int64_t value = get_int64_from_number(expr, &rtype);
		  if (rtype > 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled range is "
			                "greater than 64 bits (%u) and cannot "
			                "be safely represented.\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr), rtype);
			vlog_errors += 1;
			return;
		  }
		  if (rtype < 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled range "
			                "has an undefined bit and cannot be "
			                "represented.\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr));
			vlog_errors += 1;
			return;
		  }
		  value += lsb;
		  fprintf(vlog_out, "[%"PRId64":%"PRId64"]",
		                    value + (int64_t)(width - 1), value);
	    } else {
		  fprintf(vlog_out, "[<invalid>:<invalid>]");
		  fprintf(stderr, "%s:%u: vlog95 error: Indexed part-selects "
		                  "are not currently supported.\n",
		                  ivl_expr_file(expr), ivl_expr_lineno(expr));
		  vlog_errors += 1;
	    }
      } else {
	    if (ivl_expr_type(expr) == IVL_EX_NUMBER) {
		  int rtype;
		  int64_t value = get_int64_from_number(expr, &rtype);
		  if (rtype > 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value is "
			                "greater than 64 bits (%u) and cannot "
			                "be safely represented.\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr), rtype);
			vlog_errors += 1;
			return;
		  }
		  if (rtype < 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value "
			                "has an undefined bit and cannot be "
			                "represented.\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr));
			vlog_errors += 1;
			return;
		  }
		  value = (int64_t)lsb - value;
		  fprintf(vlog_out, "[%"PRId64":%"PRId64"]",
		                    value - (int64_t)(width - 1), value);
	    } else {
		  fprintf(vlog_out, "[<invalid>:<invalid>]");
		  fprintf(stderr, "%s:%u: vlog95 error: Indexed part-selects "
		                  "are not currently supported.\n",
		                  ivl_expr_file(expr), ivl_expr_lineno(expr));
		  vlog_errors += 1;
	    }
      }
}

void emit_scaled_expr(ivl_scope_t scope, ivl_expr_t expr, int msb, int lsb)
{
      if (msb >= lsb) {
	    if (ivl_expr_type(expr) == IVL_EX_NUMBER) {
		  int rtype;
		  int64_t value = get_int64_from_number(expr, &rtype);
		  if (rtype > 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value is "
			                "greater than 64 bits (%u) and cannot "
			                "be safely represented.\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr), rtype);
			vlog_errors += 1;
			return;
		  }
		  if (rtype < 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value "
			                "has an undefined bit and cannot be "
			                "represented.\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr));
			vlog_errors += 1;
			return;
		  }
		  value += lsb;
		  fprintf(vlog_out, "%"PRId64, value);
	    } else {
		  int64_t scale_val;
		  int rtype;
		    /* This is as easy as removing the addition/subtraction
		     * that was added to scale the value to be zero based,
		     * but we need to verify that the scaling value is
		     * correct first. */
		  if ((ivl_expr_type(expr) != IVL_EX_BINARY) ||
		      ((ivl_expr_opcode(expr) != '+') &&
		       (ivl_expr_opcode(expr) != '-')) ||
		      (ivl_expr_type(ivl_expr_oper2(expr)) != IVL_EX_NUMBER)) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value "
			                "expression/value cannot be scaled.\n ",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr));
			vlog_errors += 1;
			return;
		  }
		  scale_val = get_int64_from_number(ivl_expr_oper2(expr),
		                                    &rtype);
		  if (rtype > 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value "
			                "expression/value scale coefficient "
			                "was greater then 64 bits (%d).\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr),
			                rtype);
			vlog_errors += 1;
			return;
		  }
		  if (rtype < 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value "
			                "expression/value scale coefficient "
			                "has an undefined bit.\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr));
			vlog_errors += 1;
			return;
		  }
		  if (ivl_expr_opcode(expr) == '+') scale_val *= -1;
		  if (lsb !=  scale_val) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value "
			                "expression/value scale coefficient "
			                "did not match expected value "
			                "(%d != %"PRIu64").\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr),
			                lsb, scale_val);
			vlog_errors += 1;
			return;
		  }
		  emit_expr(scope, ivl_expr_oper1(expr), 0);
	    }
      } else {
	    if (ivl_expr_type(expr) == IVL_EX_NUMBER) {
		  int rtype;
		  int64_t value = get_int64_from_number(expr, &rtype);
		  if (rtype > 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value is "
			                "greater than 64 bits (%u) and cannot "
			                "be safely represented.\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr), rtype);
			vlog_errors += 1;
			return;
		  }
		  if (rtype < 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value "
			                "has an undefined bit and cannot be "
			                "represented.\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr));
			vlog_errors += 1;
			return;
		  }
		  value = (int64_t)lsb - value;
		  fprintf(vlog_out, "%"PRId64, value);
	    } else {
		  int64_t scale_val;
		  int rtype;
		    /* This is as easy as removing the addition/subtraction
		     * that was added to scale the value to be zero based,
		     * but we need to verify that the scaling value is
		     * correct first. */
		  if ((ivl_expr_type(expr) != IVL_EX_BINARY) ||
		      ((ivl_expr_opcode(expr) != '+') &&
		       (ivl_expr_opcode(expr) != '-')) ||
		      (ivl_expr_type(ivl_expr_oper1(expr)) != IVL_EX_NUMBER)) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value "
			                "expression/value cannot be scaled.\n ",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr));
			vlog_errors += 1;
			return;
		  }
		  scale_val = get_int64_from_number(ivl_expr_oper1(expr),
		                                    &rtype);
		  if (rtype > 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value "
			                "expression/value scale coefficient "
			                "was greater then 64 bits (%d).\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr),
			                rtype);
			vlog_errors += 1;
			return;
		  }
		  if (rtype < 0) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value "
			                "expression/value scale coefficient "
			                "has an undefined bit.\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr));
			vlog_errors += 1;
			return;
		  }
		  if (ivl_expr_opcode(expr) == '+') scale_val *= -1;
		  if (lsb !=  scale_val) {
			fprintf(vlog_out, "<invalid>");
			fprintf(stderr, "%s:%u: vlog95 error: Scaled value "
			                "expression/value scale coefficient "
			                "did not match expected value "
			                "(%d != %"PRIu64").\n",
			                ivl_expr_file(expr),
			                ivl_expr_lineno(expr),
			                lsb, scale_val);
			vlog_errors += 1;
			return;
		  }
		  emit_expr(scope, ivl_expr_oper2(expr), 0);
	    }
      }
}

// HERE: Do we need the scope to know which name to use?
void emit_name_of_nexus(ivl_nexus_t nex)
{
      fprintf(vlog_out, "<missing>");
}

/*
 * This function traverses the scope tree looking for the enclosing module
 * scope. When it is found the module scope is returned.
 */
ivl_scope_t get_module_scope(ivl_scope_t scope)
{

      while (ivl_scope_type(scope) != IVL_SCT_MODULE) {
	    ivl_scope_t pscope = ivl_scope_parent(scope);
	    assert(pscope);
	    scope = pscope;
      }
      return scope;
}

/*
 * This routine emits the appropriate string to call the call_scope from the
 * given scope. If the module scopes for the two match then do nothing. If
 * the module scopes are different, but the call_scope begins with the
 * entire module scope of scope then we can trim the top off the call_scope
 * (it is a sub-scope of the module that contains scope). Otherwise we need
 * to print the entire path of call_scope.
 */
void emit_scope_module_path(ivl_scope_t scope, ivl_scope_t call_scope)
{
      ivl_scope_t mod_scope = get_module_scope(scope);
      ivl_scope_t call_mod_scope = get_module_scope(call_scope);
      if (mod_scope != call_mod_scope) {
	      /* Trim off the top of the call name if it exactly matches
	       * the module scope of the caller. */
	    char *sc_name = strdup(ivl_scope_name(mod_scope));
            const char *sc_ptr = sc_name;
	    char *call_name = strdup(ivl_scope_name(call_mod_scope));
	    const char *call_ptr = call_name;
	    while ((*sc_ptr == *call_ptr) &&
	           (*sc_ptr != 0) && (*call_ptr != 0)) {
		  sc_ptr += 1;
		  call_ptr += 1;
	    }
	    if (*sc_ptr == 0) {
		  assert(*call_ptr == '.');
		  call_ptr += 1;
	    } else {
		  call_ptr = call_name;
	    }
	    fprintf(vlog_out, "%s.", call_ptr);
	    free(sc_name);
	    free(call_name);
      }
}

/*
 * This routine emits the appropriate string to call the call_scope from the
 * given scope. If the module scopes for the two match then just return the
 * base name of the call_scope. If the module scopes are different, but the
 * call_scope begins with the entire module scope of scope then we can trim
 * the top off the call_scope (it is a sub-scope of the module that contains
 * scope). Otherwise we need to print the entire path of call_scope.
 */
void emit_scope_path(ivl_scope_t scope, ivl_scope_t call_scope)
{
      ivl_scope_t mod_scope = get_module_scope(scope);
      ivl_scope_t call_mod_scope = get_module_scope(call_scope);
      if (mod_scope == call_mod_scope) {
	    fprintf(vlog_out, "%s", ivl_scope_basename(call_scope));
      } else {
	      /* Trim off the top of the call name if it exactly matches
	       * the module scope of the caller. */
	    char *sc_name = strdup(ivl_scope_name(mod_scope));
            const char *sc_ptr = sc_name;
	    char *call_name = strdup(ivl_scope_name(call_scope));
	    const char *call_ptr = call_name;
	    while ((*sc_ptr == *call_ptr) &&
	           (*sc_ptr != 0) && (*call_ptr != 0)) {
		  sc_ptr += 1;
		  call_ptr += 1;
	    }
	    if (*sc_ptr == 0) {
		  assert(*call_ptr == '.');
		  call_ptr += 1;
	    } else {
		  call_ptr = call_name;
	    }
	    fprintf(vlog_out, "%s", call_ptr);
	    free(sc_name);
	    free(call_name);
      }
}

/*
 * Extract an uint64_t value from the given number expression. If the result
 * type is 0 then the returned value is valid. If it is positive then the
 * value was too large and if it is negative then the value had undefined
 * bits.
 */
uint64_t get_uint64_from_number(ivl_expr_t expr, int *result_type)
{
      unsigned nbits = ivl_expr_width(expr);
      unsigned trim_wid = nbits - 1;
      const char *bits = ivl_expr_bits(expr);
      unsigned idx;
      uint64_t value = 0;
      assert(ivl_expr_type(expr) == IVL_EX_NUMBER);
      assert(! ivl_expr_signed(expr));
	/* Trim any '0' bits from the MSB. */
      for (/* none */; trim_wid > 0; trim_wid -= 1) {
	    if ('0' != bits[trim_wid]) {
		  trim_wid += 1;
		  break;
	    }
      }
      if (trim_wid < nbits) trim_wid += 1;
	/* Check to see if the value is too large. */
      if (trim_wid > 64U) {
	    *result_type = trim_wid;
	    return 0;
      }
	/* Now build the value from the bits. */
      for (idx = 0; idx < trim_wid; idx += 1) {
	    if (bits[idx] == '1') value |= (uint64_t)1 << idx;
	    else if (bits[idx] != '0') {
		  *result_type = -1;
		  return 0;
	    }
      }
      *result_type = 0;
      return value;
}

/*
 * Extract an int64_t value from the given number expression. If the result
 * type is 0 then the returned value is valid. If it is positive then the
 * value was too large and if it is negative then the value had undefined
 * bits.
 */
int64_t get_int64_from_number(ivl_expr_t expr, int *result_type)
{
      unsigned is_signed = ivl_expr_signed(expr);
      unsigned nbits = ivl_expr_width(expr);
      unsigned trim_wid = nbits - 1;
      const char *bits = ivl_expr_bits(expr);
      const char msb = is_signed ? bits[trim_wid] : '0';
      unsigned idx;
      int64_t value = 0;
      assert(ivl_expr_type(expr) == IVL_EX_NUMBER);
	/* Trim any duplicate bits from the MSB. */
      for (/* none */; trim_wid > 0; trim_wid -= 1) {
	    if (msb != bits[trim_wid]) {
		  trim_wid += 1;
		  break;
	    }
      }
      if (trim_wid < nbits) trim_wid += 1;
	/* Check to see if the value is too large. */
      if (trim_wid > 64U) {
	    *result_type = trim_wid;
	    return 0;
      }
	/* Now build the value from the bits. */
      for (idx = 0; idx < trim_wid; idx += 1) {
	    if (bits[idx] == '1') value |= (int64_t)1 << idx;
	    else if (bits[idx] != '0') {
		  *result_type = -1;
		  return 0;
	    }
      }
	/* Sign extend as needed. */
      if (is_signed && (msb == '1') && (trim_wid < 64U)) {
	    value |= ~(((int64_t)1 << trim_wid) - (int64_t)1);
      }
      *result_type = 0;
      return value;
}

/*
 * Extract an int32_t value from the given number expression. If the result
 * type is 0 then the returned value is valid. If it is positive then the
 * value was too large and if it is negative then the value had undefined
 * bits.
 */
int32_t get_int32_from_number(ivl_expr_t expr, int *result_type)
{
      unsigned is_signed = ivl_expr_signed(expr);
      unsigned nbits = ivl_expr_width(expr);
      unsigned trim_wid = nbits - 1;
      const char *bits = ivl_expr_bits(expr);
      const char msb = is_signed ? bits[trim_wid] : '0';
      unsigned idx;
      int32_t value = 0;
      assert(ivl_expr_type(expr) == IVL_EX_NUMBER);
	/* Trim any duplicate bits from the MSB. */
      for (/* none */; trim_wid > 0; trim_wid -= 1) {
	    if (msb != bits[trim_wid]) {
		  trim_wid += 1;
		  break;
	    }
      }
      if (trim_wid < nbits) trim_wid += 1;
	/* Check to see if the value is too large. */
      if (trim_wid > 32U) {
	    *result_type = trim_wid;
	    return 0;
      }
	/* Now build the value from the bits. */
      for (idx = 0; idx < trim_wid; idx += 1) {
	    if (bits[idx] == '1') value |= (int32_t)1 << idx;
	    else if (bits[idx] != '0') {
		  *result_type = -1;
		  return 0;
	    }
      }
	/* Sign extend as needed. */
      if (is_signed && (msb == '1') && (trim_wid < 32U)) {
	    value |= ~(((int32_t)1 << trim_wid) - (int32_t)1);
      }
      *result_type = 0;
      return value;
}