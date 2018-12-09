#include <Functions/IFunction.h>
#include <Functions/FunctionFactory.h>
#include <Functions/GatherUtils/GatherUtils.h>
#include <DataTypes/DataTypeArray.h>
#include <DataTypes/getLeastSupertype.h>
#include <Interpreters/castColumn.h>
#include <Columns/ColumnArray.h>
#include <Columns/ColumnConst.h>
#include <Common/typeid_cast.h>
#include <ext/range.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
    extern const int NUMBER_OF_ARGUMENTS_DOESNT_MATCH;
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
}


/// arrayConcat(arr1, ...) - concatenate arrays.
class FunctionArrayConcat : public IFunction
{
public:
    static constexpr auto name = "arrayConcat";
    static FunctionPtr create(const Context & context) { return std::make_shared<FunctionArrayConcat>(context); }
    FunctionArrayConcat(const Context & context) : context(context) {}

    String getName() const override { return name; }

    String getSignature() const override { return "f(Array(T1), ...) -> Array(leastSuperType(T1, ...))"; }

    void executeImpl(Block & block, const ColumnNumbers & arguments, size_t result, size_t input_rows_count) override
    {
        const DataTypePtr & return_type = block.getByPosition(result).type;

        if (return_type->onlyNull())
        {
            block.getByPosition(result).column = return_type->createColumnConstWithDefaultValue(input_rows_count);
            return;
        }

        auto result_column = return_type->createColumn();

        size_t rows = input_rows_count;
        size_t num_args = arguments.size();

        Columns preprocessed_columns(num_args);

        for (size_t i = 0; i < num_args; ++i)
        {
            const ColumnWithTypeAndName & arg = block.getByPosition(arguments[i]);
            ColumnPtr preprocessed_column = arg.column;

            if (!arg.type->equals(*return_type))
                preprocessed_column = castColumn(arg, return_type, context);

            preprocessed_columns[i] = std::move(preprocessed_column);
        }

        std::vector<std::unique_ptr<GatherUtils::IArraySource>> sources;

        for (auto & argument_column : preprocessed_columns)
        {
            bool is_const = false;

            if (auto argument_column_const = typeid_cast<const ColumnConst *>(argument_column.get()))
            {
                is_const = true;
                argument_column = argument_column_const->getDataColumnPtr();
            }

            if (auto argument_column_array = typeid_cast<const ColumnArray *>(argument_column.get()))
                sources.emplace_back(GatherUtils::createArraySource(*argument_column_array, is_const, rows));
            else
                throw Exception{"Arguments for function " + getName() + " must be arrays.", ErrorCodes::LOGICAL_ERROR};
        }

        auto sink = GatherUtils::createArraySink(typeid_cast<ColumnArray &>(*result_column), rows);
        GatherUtils::concat(sources, *sink);

        block.getByPosition(result).column = std::move(result_column);
    }

    bool useDefaultImplementationForConstants() const override { return true; }

private:
    const Context & context;
};


void registerFunctionArrayConcat(FunctionFactory & factory)
{
    factory.registerFunction<FunctionArrayConcat>();
}

}
