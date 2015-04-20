/**
 * \file Column.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for managing columns
 *
 * Copyright (C) 2015 CESNET, z.s.p.o.
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

#ifndef COLUMN_H_
#define COLUMN_H_

#include "typedefs.h"
#include "Values.h"
#include "Cursor.h"
#include "plugins/plugin_header.h"

namespace fbitdump {

/* Cursor depends on Column class */
class Cursor;

/**
 * \brief Class with information about one column
 *
 * The class can initialise itself from XML configuration.
 * It is also used for column separators (like '->' or ': ' in output)
 * Public methods allow to get values of individual properties
 * Using getValue method an value from cursor can be accessed for this column
 */
class Column
{
public:

	/**
	 * \brief Returns column name (that should be printed in header)
	 *
	 * @return Column name
	 */
	const std::string getName() const;

	/**
	 * \brief Sets name to "name"
	 * @param name New name for the column
	 */
	void setName(std::string name);

	/**
	 * \brief Returns set of column aliases
	 *
	 * @return Set of column aliases
	 */
	const stringSet getAliases() const;

	/**
	 * \brief Returns width of the column (specified in XML)
	 * @return width of the column
	 */
	int getWidth() const;

	/**
	 * \brief Returns true when column should be aligned to the left
	 * @return true when column should be aligned to the left, false otherwise
	 */
	bool getAlignLeft() const;

	/**
	 * \brief Class constructor, initialise column from xml configuration
	 *
	 * @param doc pugi xml document
	 * @param alias alias of the new column
	 * @param aggregate Aggregate mode
	 * @return true when initialization completed, false on error
	 */
	Column(const pugi::xml_document &doc, const std::string &alias, bool aggregate);

	/**
	 * \brief Class constructor, initialises only column name
	 *
	 * This is used for column separators (spaces, "->", ":", etc ...)
	 *
	 * @param name Name of the column (to be diplayed in header)
	 */
	Column(const std::string &name);

	/**
	 * \brief Returns value of current column in row specified by cursor
	 *
	 * structure values contains required value in form of union
	 *
	 * @param cur cursor pointing to current row
	 * @return values structure containing required value
	 */
	const Values* getValue(const Cursor *cur) const;

	/**
	 * \brief Can this column be used in aggregation?
	 * @return true when column is aggregatable
	 */
	bool getAggregate() const;

	/**
	 * \brief Returns set of table column names that this column works with
	 *
	 * @return Set of table column names
	 */
	const stringSet getColumns() const;

	/**
	 * \brief Returns string to print when value is not available
	 *
	 * @return string to print when value is not available
	 */
	const std::string getNullStr() const;

	/**
	 * \brief Returns semantics of the column
	 * This is used to specify formatting of the column
	 *
	 * @return semantics of the column
	 */
	const std::string getSemantics() const;

	/**
	 * \brief Returns semantics parameters
	 *
	 * @return semantics parameters
	 */
	const std::string getSemanticsParams() const { return ast->semanticsParams; }

	/**
	 * \brief Returns true if column is a separator column
	 *
	 * @return returns true if column is a separator column, false otherwise
	 */
	bool isSeparator() const;

	/**
	 * \brief Returns true when column represents an operation
	 *
	 * @return true when column represents an operation
	 */
	bool isOperation() const;

	/**
	 * \brief Return name of the file, that contains data for this column
	 *
	 * @return this->element
	 */
	const std::string getElement() const;

	/**
	 * \brief Returns true when column is configured as summary column
	 *
	 * @return true when column is configured as summary column
	 */
	bool isSummary() const;

	/**
	 * \brief Class destructor
	 * Frees AST
	 */
	~Column();

	void (*format)(const plugin_arg_t * val, int, char*, void *conf);
	void (*parse)(char *input, char *out, void *conf);
	void *pluginConf;

        /**
         * \brief Returns summary type
         * 
         * \return summary type
         */
        std::string getSummaryType() const { return summaryType; }
        
        /**
         * \brief Return true if summary type is sum
         * 
         * \return true if summary type is sum
         */
        bool isSumSummary() const{ return summaryType == "sum"; }
        
        /**
         * \brief Return true if summary type is avg
         * 
         * \return true if summary type is avg
         */
        bool isAvgSummary() const { return summaryType == "avg"; }
        
        /**
         * \brief Returns name for select clause
         * 
         * \return name for select clause
         */
        std::string getSelectName() const { return selectName; }
        
        /**
         * \brief Returns number of parts
         * 
         * @return number of parts
         */
        int getParts() const { return this->ast->parts; }
        
        /**
         * \brief Returns aggregation function
         * 
         * @return aggregation function
         */
        std::string getAggregateType() const { return ast->aggregation; }
private:

	/**
	 * \brief Abstract syntax tree structure
	 *
	 * Describes the way that column value is constructed from database columns
	 */
	struct AST
	{
		/**
		 * \brief types for AST structure
		 */
		enum astTypes
		{
			valueType,   //!< value
			operationType//!< operation
		};

		astTypes type; /**< AST type */
		unsigned char operation; /**< one of '/', '*', '-', '+' */
		std::string semantics; /**< semantics of the column */
		std::string semanticsParams; /**< semantics parameters */
		std::string value; /**< value (column name) */
		std::string aggregation; /**< how to aggregate this column */
		int parts; /**< number of parts of column (ipv6 => e0id27p0 and e0id27p1)*/
		AST *left; /**< left subtree */
		AST *right; /**< right subtree */

		stringSet astColumns; /**< Cached columns set (computed in Column::getColumns(AST*)) */
		bool cached;

		/**
		 * \brief AST constructor - sets default values
		 */
		AST(): type(valueType), operation('+'), parts(1), left(NULL), right(NULL), cached(false) {}

		/**
		 * \brief AST destructor
		 */
		~AST()
		{
			delete left;
			delete right;
		}
	};

	/**
	 * \brief Initialise column from xml configuration
	 *
	 * @param doc pugi xml document
	 * @param alias alias of the new column
	 * @param aggregate Aggregate mode
	 */
	void init(const pugi::xml_document &doc, const std::string &alias, bool aggregate);

	/**
	 * \brief Evaluates AST against data in cursor
	 *
	 * @param ast AST to evaluate
	 * @param cur Cursor with data
	 * @return returns values structure
	 */
	const Values *evaluate(AST *ast, const Cursor *cur) const;

	/**
	 * \brief Set AST for this column
	 *
	 * @param ast AST for this column
	 */
	void setAST(AST *ast);

	/**
	 * \brief Parse semantics from xml and fill in members in AST
	 */ 
	void parseSemantics(AST *ast, std::string semantics);

	/**
	 * \brief Create element of type value from XMLnode element
	 *
	 * @param element XML node element
	 * @param doc XML document with configuration
	 * @return AST structure of created element
	 */
	AST* createValueElement(pugi::xml_node element, const pugi::xml_document &doc);

	/**
	 * \brief Create element of type operation from XMLnode element
	 *
	 * @param operation XML node element
	 * @param doc XML document with configuration
	 * @return AST structure of created operation
	 */
	AST* createOperationElement(pugi::xml_node operation, const pugi::xml_document &doc);

	/**
	 * \brief Return column names used in this AST
	 *
	 * @param ast to go through
	 * @return Set of column names
	 */
	const stringSet& getColumns(AST* ast) const;

	/**
	 * \brief Is AST aggregable?
	 *
	 * @param ast AST to check
	 * @return true when AST is aggregable
	 */
	bool getAggregate(AST* ast) const;

	/**
	 * \brief Add alias to current set of aliases
	 * @param alias Aliass to add
	 */
	void addAlias(std::string alias);

	/**
	 * \brief Set column width to "width"
	 * @param width New width of the column
	 */
	void setWidth(int width);

	/**
	 * \brief Set column alignment
	 * @param alignLeft True when column should be aligned to the left
	 */
	void setAlignLeft(bool alignLeft);

	/**
	 * \brief Sets columns aggregation mode
	 *
	 * This is important for function getColumns(), which should know
	 * whether to return column names with aggregation function i.e. sum(e0id1)
	 *
	 * @param aggregation
	 */
	void setAggregation(bool aggregation);


	std::string nullStr;    /**< String to print when columns has no value */
	std::string name;       /**< name of the column  */
	int width;              /**< Width of the column */
	bool alignLeft;         /**< Align column to left?*/
	AST *ast;               /**< Abstract syntax tree for value of this column */
	stringSet aliases;      /**< Aliases of the column*/
	bool aggregation;       /**< Determines whether column is in aggregation mode */
	std::string element;    /**< name of the file, which contains actual data for this column */
	bool summary;			/**< Is this a summary column? */
        std::string summaryType;/**< summary type - sum or avg */
        std::string selectName; /**< name for select clause */

}; /* end of Column class */

} /* end of fbitdump namespace */

#endif /* COLUMN_H_ */
