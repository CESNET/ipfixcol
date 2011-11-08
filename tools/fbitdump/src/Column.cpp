/**
 * \file Column.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class for management of columns
 *
 * Copyright (C) 2011 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include <getopt.h>
#include <iostream>
#include "Column.h"

namespace ipfixdump
{

bool Column::init(pugi::xml_document &doc, std::string alias, bool aggregate)
{

	/* search xml for an alias */
	pugi::xpath_node column = doc.select_single_node(("/columns/column[alias='"+alias+"']").c_str());
	/* check what we found */
	if (column == NULL) {
		std::cerr << "Column '" << alias << "' not defined" << std::endl;
		return false;
	}

	/* set default value */
	if (column.node().child("default-value") != NULL) {
		this->nullStr = column.node().child_value("default-value");
	}

	this->setName(column.node().child_value("name"));
	this->setAggregation(aggregate);

#ifdef DEBUG
	std::cerr << "Creating column '" << this->name << "'" << std::endl;
#endif

	/* set alignment */
	if (column.node().child("alignLeft") != NULL) {
		this->setAlignLeft(true);
	}

	/* set width */
	if (column.node().child("width") != NULL) {
		this->setWidth(atoi(column.node().child_value("width")));
	}

	/* set value according to type */
	if (column.node().child("value").attribute("type").value() == std::string("plain")) {
		/* simple element */
		this->setAST(createValueElement(column.node().child("value").child("element"), doc));
	} else if (column.node().child("value").attribute("type").value() == std::string("operation")) {
		/* operation */
		this->setAST(createOperationElement(column.node().child("value").child("operation"), doc));
	}

	/* add aliases from XML to column (with starting %) */
	pugi::xpath_node_set aliases = column.node().select_nodes("alias");
	for (pugi::xpath_node_set::const_iterator it = aliases.begin(); it != aliases.end(); ++it) {
		this->addAlias(it->node().child_value());
	}
	return true;
}

AST* Column::createValueElement(pugi::xml_node element, pugi::xml_document &doc)
{

	/* when we have alias, go down one level */
	if (element.child_value()[0] == '%') {
		pugi::xpath_node el = doc.select_single_node(
				("/columns/column[alias='"
						+ std::string(element.child_value())
						+ "']/value/element").c_str());
		return createValueElement(el.node(), doc);
	}

	/* create the element */
	AST *ast = new AST;

	ast->type = ipfixdump::value;
	ast->value = element.child_value();
	ast->semantics = element.attribute("semantics").value();
	if (element.attribute("parts")) {
		ast->parts = atoi(element.attribute("parts").value());
	}
	if (element.attribute("aggregation")) {
		ast->aggregation = element.attribute("aggregation").value();
	}

	return ast;
}

AST* Column::createOperationElement(pugi::xml_node operation, pugi::xml_document &doc)
{

	AST *ast = new AST;
	pugi::xpath_node arg1, arg2;
	std::string type;

	/* set type and operation */
	ast->type = ipfixdump::operation;
	ast->operation = operation.attribute("name").value()[0];
	ast->semantics = operation.attribute("semantics").value();

	/* get argument nodes */
	arg1 = doc.select_single_node(("/columns/column[alias='"+ std::string(operation.child_value("arg1"))+ "']").c_str());
	arg2 = doc.select_single_node(("/columns/column[alias='"+ std::string(operation.child_value("arg2"))+ "']").c_str());

	/* get argument type */
	type = arg1.node().child("value").attribute("type").value();

	/* add argument to AST */
	if (type == "operation") {
		ast->left = createOperationElement(arg1.node().child("value").child("operation"), doc);
	} else if (type == "plain"){
		ast->left = createValueElement(arg1.node().child("value").child("element"), doc);
	} else {
		std::cerr << "Value of type operation contains node of type " << type << std::endl;
	}

	/* same for the second argument */
	type = arg2.node().child("value").attribute("type").value();

	if (type == "operation") {
		ast->right = createOperationElement(arg2.node().child("value").child("operation"), doc);
	} else if (type == "plain"){
		ast->right = createValueElement(arg2.node().child("value").child("element"), doc);
	} else {
		std::cerr << "Value of type operation contains node of type '" << type << "'" << std::endl;
	}

	return ast;
}

std::string Column::getName()
{
	return this->name;
}

void Column::setName(std::string name)
{
	this->name = name;
}

stringSet Column::getAliases() {
	return this->aliases;
}

void Column::addAlias(std::string alias)
{
	this->aliases.insert(alias);
}

int Column::getWidth()
{
	return this->width;
}

void Column::setWidth(int width)
{
	this->width = width;
}

bool Column::getAlignLeft()
{
	return this->alignLeft;
}

void Column::setAlignLeft(bool alignLeft)
{
	this->alignLeft = alignLeft;
}


//std::string Column::getValue(Cursor *cur, bool plainNumbers) {
//	std::string valueStr;
//	std::stringstream ss;
//	values *val;
//
//	/* check whether we have name column */
//	if (ast == NULL) {
//		return name;
//	}
//
//	val = evaluate(this->ast, cur);
//
//	/* check for missing column */
//	if (val == NULL) {
//		return this->nullStr;
//	}
//
//	if (!this->ast->semantics.empty() && this->ast->semantics != "flows") {
//		if (this->ast->semantics == "ipv4") {
//			valueStr = printIPv4(val->value[0].uint32);
//		} else if (this->ast->semantics == "timestamp") {
//			if (plainNumbers == true) {
//				valueStr = val->value[0].uint64;
//			} else {
//				valueStr = printTimestamp(val->value[0].uint64);
//			}
//		} else if (this->ast->semantics == "ipv6") {
//			valueStr = printIPv6(val->value[0].uint64, val->value[1].uint64);
//		} else if (this->ast->semantics == "protocol") {
//			if (!plainNumbers) {
//				valueStr = protocols[val->value[0].uint8];
//			} else {
//				ss << (uint16_t) val->value[0].uint8;
//				valueStr = ss.str();
//			}
//		} else if (this->ast->semantics == "tcpflags") {
//			valueStr = printTCPFlags(val->value[0].uint8);
//		}
//	} else {
//		valueStr = val->toString();
//	}
//
//	/* clean value variable */
//	delete val;
//
//	return valueStr;

values* Column::getValue(Cursor *cur)
{
	return evaluate(this->ast, cur);
}

values *Column::evaluate(AST *ast, Cursor *cur)
{
	values *retVal = NULL;

	/* check input AST */
	if (ast == NULL) {
		return NULL;
	}

	/* evaluate AST */
	switch (ast->type) {
		case ipfixdump::value:{
			retVal = new values;
			int part=0;
			stringSet &tmpSet = this->getColumns(ast);

			for (stringSet::iterator it = tmpSet.begin(); it != tmpSet.end(); it++) {
				/* get value from table */
				if (!cur->getColumn(*it, *retVal, part)) {
					delete retVal;
					return NULL;
				}
				part++;
			}

			break;}
		case ipfixdump::operation:
			values *left, *right;

			left = evaluate(ast->left, cur);
			right = evaluate(ast->right, cur);

			if (left != NULL && right != NULL) {
				retVal = performOperation(left, right, ast->operation);
			}
			/* clean up */
			delete left;
			delete right;

			break;
		default:
			std::cerr << "Unknown AST type" << std::endl;
			break;
	}

	return retVal;
}

values* Column::performOperation(values *left, values *right, unsigned char op)
{
	values *result = new values;
	/* TODO add some type checks maybe... */
	switch (op) {
				case '+':
					result->type = ibis::ULONG;
					result->value[0].uint64 = left->toDouble() + right->toDouble();
					break;
				case '-':
					result->type = ibis::ULONG;
					result->value[0].uint64 = left->toDouble() - right->toDouble();
					break;
				case '*':
					result->type = ibis::ULONG;
					result->value[0].uint64 = left->toDouble() * right->toDouble();
					break;
				case '/':
					result->type = ibis::ULONG;
					if (right->toDouble() == 0) {
						result->value[0].uint64 = 0;
					} else {
						result->value[0].uint64 = left->toDouble() / right->toDouble();
					}
					break;
				}
	return result;
}

Column::~Column()
{
	delete this->ast;
}

stringSet& Column::getColumns(AST* ast)
{
	/* use cached values if possible */
	if (ast->cached) return ast->astColumns;

	if (ast->type == ipfixdump::value) {
		if (ast->semantics != "flows") {
			if (ast->parts > 1) {
				for (int i = 0; i < ast->parts; i++) {
					std::stringstream strStrm;
					strStrm << ast->value << "p" << i;
					ast->astColumns.insert(strStrm.str());
				}
			} else {
				if (this->aggregation && !ast->aggregation.empty()) {
					ast->astColumns.insert(ast->aggregation + "(" + ast->value + ")");
				} else {
					ast->astColumns.insert(ast->value);
				}
			}
		} else { /* flows need this (on aggregation value must be set to count) */
			ast->astColumns.insert("count(*)");
			ast->value = "*";
		}
	} else { /* operation */
		stringSet ls, rs;
		ls = getColumns(ast->left);
		rs = getColumns(ast->right);
		ast->astColumns.insert(ls.begin(), ls.end());
		ast->astColumns.insert(rs.begin(), rs.end());
	}

	ast->cached = true;

	return ast->astColumns;
}

stringSet Column::getColumns()
{
	/* check for name column */
	if (this->ast == NULL) {
		return stringSet();
	}

	return getColumns(this->ast);
}

bool Column::getAggregate()
{

	/* Name columns are not aggregable */
	if (this->ast != NULL && getAggregate(this->ast)) {
			return true;
	}

	return false;
}

bool Column::getAggregate(AST* ast)
{

	/* AST type must be 'value' and aggregation string must not be empty */
	if (ast->type == ipfixdump::value) {
		if (!ast->aggregation.empty()) {
			return true;
		}
	/* or both sides of operation must be aggregable */
	} else if (ast->type == ipfixdump::operation) {
		return getAggregate(ast->left) && getAggregate(ast->right);
	}

	/* all other cases */
	return false;
}

void Column::setAST(AST *ast)
{
	this->ast = ast;
}

void Column::setAggregation(bool aggregation)
{
	this->aggregation = aggregation;
}

std::string Column::getNullStr()
{
	return this->nullStr;
}

std::string Column::getSemantics()
{
	if (this->ast == NULL) {
		return "";
	}

	return this->ast->semantics;
}

bool Column::isSeparator() {
	if (this->ast == NULL) return true;
	return false;
}

bool Column::isOperation() {
	if (this->ast != NULL && this->ast->type == ipfixdump::operation) {
		return true;
	}

	return false;
}

Column::Column(): nullStr("NULL"), width(0), alignLeft(false), ast(NULL), aggregation(false) {}

}
