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

namespace fbitdump
{

void Column::init(const pugi::xml_document &doc, const std::string &alias, bool aggregate) throw(std::invalid_argument)
{
	this->format = NULL;

	/* search xml for an alias */
	pugi::xpath_node column = doc.select_single_node(("/configuration/columns/column[alias='"+alias+"']").c_str());
	/* check what we found */
	if (column == NULL) {
		throw std::invalid_argument(std::string("Column '") + alias + "' not defined");
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

	/* element is name of the file, which contains actual data for this column */
	if (column.node().child("value").child("element") != 0) {
		this->element = column.node().child("value").child_value("element");
	}

	/* check whether this is a summary column */
	pugi::xpath_node_set sumColumns = doc.select_nodes("/configuration/summary/column");
	for (stringSet::const_iterator it = this->aliases.begin(); it != this->aliases.end(); it++) {
		for (pugi::xpath_node_set::const_iterator i = sumColumns.begin(); i != sumColumns.end(); ++i) {
			if (*it == i->node().child_value()) {
				this->summary = true;
			}
		}
	}
}

Column::AST* Column::createValueElement(pugi::xml_node element, const pugi::xml_document &doc)
{

	/* when we have alias, go down one level */
	if (element.child_value()[0] == '%') {
		pugi::xpath_node el = doc.select_single_node(
				("/configuration/columns/column[alias='"
						+ std::string(element.child_value())
						+ "']/value/element").c_str());
		return createValueElement(el.node(), doc);
	}

	/* create the element */
	AST *ast = new AST;

	ast->type = fbitdump::Column::AST::valueType;
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

Column::AST* Column::createOperationElement(pugi::xml_node operation, const pugi::xml_document &doc)
{

	AST *ast = new AST;
	pugi::xpath_node arg1, arg2;
	std::string type;

	/* set type and operation */
	ast->type = fbitdump::Column::AST::operationType;
	ast->operation = operation.attribute("name").value()[0];
	ast->semantics = operation.attribute("semantics").value();

	/* get argument nodes */
	arg1 = doc.select_single_node(("/configuration/columns/column[alias='"+ std::string(operation.child_value("arg1"))+ "']").c_str());
	arg2 = doc.select_single_node(("/configuration/columns/column[alias='"+ std::string(operation.child_value("arg2"))+ "']").c_str());

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

const std::string Column::getName() const
{
	return this->name;
}

void Column::setName(std::string name)
{
	this->name = name;
}

const stringSet Column::getAliases() const
{
	return this->aliases;
}

void Column::addAlias(std::string alias)
{
	this->aliases.insert(alias);
}

int Column::getWidth() const
{
	return this->width;
}

void Column::setWidth(int width)
{
	this->width = width;
}

bool Column::getAlignLeft() const
{
	return this->alignLeft;
}

void Column::setAlignLeft(bool alignLeft)
{
	this->alignLeft = alignLeft;
}

const Values* Column::getValue(const Cursor *cur) const
{
	return evaluate(this->ast, cur);
}

const Values *Column::evaluate(AST *ast, const Cursor *cur) const
{

	/* check input AST */
	if (ast == NULL) {
		return NULL;
	}

	/* evaluate AST */
	switch (ast->type) {
		case fbitdump::Column::AST::valueType:{
			Values *retVal = new Values;
			int part=0;
			const stringSet &tmpSet = this->getColumns(ast);

			for (stringSet::const_iterator it = tmpSet.begin(); it != tmpSet.end(); it++) {
				/* get value from table */
				if (!cur->getColumn(*it, *retVal, part)) {
					delete retVal;
					return NULL;
				}
				part++;
			}

			return retVal;
			break;}
		case fbitdump::Column::AST::operationType:{
			const Values *left, *right, *retVal = NULL;

			left = evaluate(ast->left, cur);
			right = evaluate(ast->right, cur);

			if (left != NULL && right != NULL) {
				retVal = performOperation(left, right, ast->operation);
			}
			/* clean up */
			delete left;
			delete right;

			return retVal;
			break;}
		default:
			std::cerr << "Unknown AST type" << std::endl;
			break;
	}

	return NULL;
}

const Values* Column::performOperation(const Values *left, const Values *right, unsigned char op) const
{
	Values *result = new Values;
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

const stringSet& Column::getColumns(AST* ast) const
{
	/* use cached values if possible */
	if (ast->cached) return ast->astColumns;

	if (ast->type == fbitdump::Column::AST::valueType) {
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

const stringSet Column::getColumns() const
{
	/* check for name column */
	if (this->ast == NULL) {
		return stringSet();
	}

	return getColumns(this->ast);
}

bool Column::getAggregate() const
{

	/* Name columns are not aggregable */
	if (this->ast != NULL && getAggregate(this->ast)) {
			return true;
	}

	return false;
}

bool Column::getAggregate(AST* ast) const
{

	/* AST type must be 'value' and aggregation string must not be empty */
	if (ast->type == fbitdump::Column::AST::valueType) {
		if (!ast->aggregation.empty()) {
			return true;
		}
	/* or both sides of operation must be aggregable */
	} else if (ast->type == fbitdump::Column::AST::operationType) {
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

const std::string Column::getNullStr() const
{
	return this->nullStr;
}

const std::string Column::getSemantics() const
{
	if (this->ast == NULL) {
		return "";
	}

	return this->ast->semantics;
}

bool Column::isSeparator() const
{
	if (this->ast == NULL) return true;
	return false;
}

bool Column::isOperation() const
{
	if (this->ast != NULL && this->ast->type == fbitdump::Column::AST::operationType) {
		return true;
	}

	return false;
}

const std::string Column::getElement() const
{
	return this->element;
}

bool Column::isSummary() const
{
	return this->summary;
}

Column::Column(const pugi::xml_document &doc, const std::string &alias, bool aggregate) throw(std::invalid_argument):
		nullStr("NULL"), width(0), alignLeft(false), ast(NULL), aggregation(false), summary(false)
{
	this->init(doc, alias, aggregate);
}

Column::Column(const std::string &name): nullStr("NULL"), name(name), width(0), alignLeft(false), ast(NULL), aggregation(false), summary(false) {}

}
