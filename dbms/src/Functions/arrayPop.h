#include <Functions/IFunction.h>
#include <Functions/GatherUtils/GatherUtils.h>
#include <DataTypes/DataTypeArray.h>
#include <Columns/ColumnArray.h>
#include <Common/typeid_cast.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
}


class FunctionArrayPop : public IFunction
{
public:
    FunctionArrayPop(bool pop_front, const char * name) : pop_front(pop_front), name(name) {}

    String getName() const override { return name; }

    String getSignature() const override { return "f(Array(T)) -> Array(T)"; }

    void executeImpl(Block & block, const ColumnNumbers & arguments, size_t result, size_t input_rows_count) override
    {
        const auto & return_type = block.getByPosition(result).type;

        if (return_type->onlyNull())
        {
            block.getByPosition(result).column = return_type->createColumnConstWithDefaultValue(input_rows_count);
            return;
        }

        auto result_column = return_type->createColumn();

        const auto & array_column = block.getByPosition(arguments[0]).column;

        std::unique_ptr<GatherUtils::IArraySource> source;

        size_t size = array_column->size();

        if (auto argument_column_array = typeid_cast<const ColumnArray *>(array_column.get()))
            source = GatherUtils::createArraySource(*argument_column_array, false, size);
        else
            throw Exception{"First arguments for function " + getName() + " must be array.", ErrorCodes::LOGICAL_ERROR};

        auto sink = GatherUtils::createArraySink(typeid_cast<ColumnArray &>(*result_column), size);

        if (pop_front)
            GatherUtils::sliceFromLeftConstantOffsetUnbounded(*source, *sink, 1);
        else
            GatherUtils::sliceFromLeftConstantOffsetBounded(*source, *sink, 0, -1);

        block.getByPosition(result).column = std::move(result_column);
    }

    bool useDefaultImplementationForConstants() const override { return true; }
    bool useDefaultImplementationForNulls() const override { return false; }

private:
    bool pop_front;
    const char * name;
};

}
