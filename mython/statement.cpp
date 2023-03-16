#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

	using runtime::Closure;
	using runtime::Context;
	using runtime::ObjectHolder;

	namespace {
		const string ADD_METHOD = "__add__"s;
		const string INIT_METHOD = "__init__"s;
	}  // namespace

	ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
		closure[name_] = rv_->Execute(closure, context);
		return closure.at(name_);
	}

	Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
		:name_(move(var)), rv_(move(rv))
	{
	}

	VariableValue::VariableValue(const std::string& var_name)
		:name_(var_name)
	{
	}

	VariableValue::VariableValue(std::vector<std::string> dotted_ids)
		:dotted_ids_(move(dotted_ids))
	{
	}

	ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
		ObjectHolder res;

		if (!name_.empty()) {
			if (closure.count(name_)) res = closure.at(name_);
			else throw runtime_error("Unknown variable " + name_);
		}
		else if (!dotted_ids_.empty()) {
			Closure* next_closure = &closure;
			for (size_t i = 0; i < dotted_ids_.size() - 1; ++i) {
				if (!next_closure->count(dotted_ids_[i])) throw runtime_error("Unknown variable " + dotted_ids_[i]);
				res = next_closure->at(dotted_ids_[i]);
				if (res.TryAs<runtime::ClassInstance>()) {
					next_closure = &res.TryAs<runtime::ClassInstance>()->Fields();
				}
				else {
					throw runtime_error("Instance is not a class");
				}
			}
			if (!next_closure->count(dotted_ids_.back())) throw runtime_error("Unknown variable " + dotted_ids_.back());
			res = next_closure->at(dotted_ids_.back());
		}
		else {
			throw runtime_error("Unknown variable");
		}
		return res;
	}

	unique_ptr<Print> Print::Variable(const std::string& name) {
		unique_ptr<Statement> arg = make_unique<VariableValue>(name);
		return make_unique<Print>(move(arg));
	}

	Print::Print(unique_ptr<Statement> argument)
	{
		args_.push_back(move(argument));
	}

	Print::Print(vector<unique_ptr<Statement>> args)
		:args_(move(args))
	{
	}

	ObjectHolder Print::Execute(Closure& closure, Context& context) {

		for (size_t i = 0; i < args_.size(); ++i) {
			ObjectHolder obj = args_[i]->Execute(closure, context);
			if (obj) {
				obj->Print(context.GetOutputStream(), context);
			}
			else {
				context.GetOutputStream() << "None"s;
			}
			if (i != args_.size() - 1) context.GetOutputStream() << ' ';
		}
		context.GetOutputStream() << '\n';
		return {};
	}

	MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
		std::vector<std::unique_ptr<Statement>> args)
		:object_(move(object)), method_(move(method)), args_(move(args))
	{
	}

	ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
		runtime::ClassInstance* class_obj = object_->Execute(closure, context).TryAs<runtime::ClassInstance>();
		if (class_obj && class_obj->HasMethod(method_, args_.size())) {

			vector<ObjectHolder> args_executed(args_.size());
			auto it = args_.begin();
			for (ObjectHolder& arg_obj : args_executed) {
				arg_obj = move((*it)->Execute(closure, context));
				++it;
			}
			return class_obj->Call(method_, args_executed, context);
		}
		else {
			return {};
		}
	}

	ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
		ObjectHolder res = arg_->Execute(closure, context);
		stringstream temp_stream;
		if (res) {
			res->Print(temp_stream, context);
		}
		else {
			temp_stream << "None";
		}
		return ObjectHolder::Own(runtime::String(temp_stream.str()));
	}

	ObjectHolder Add::Execute(Closure& closure, Context& context) {
		ObjectHolder lhs_obj = lhs_->Execute(closure, context);
		ObjectHolder rhs_obj = rhs_->Execute(closure, context);

		auto lhs_str = lhs_obj.TryAs<runtime::String>();
		auto rhs_str = rhs_obj.TryAs<runtime::String>();
		if (lhs_str && rhs_str) {
			return ObjectHolder::Own(runtime::String{ lhs_str->GetValue() + rhs_str->GetValue() });
		}

		auto lhs_num = lhs_obj.TryAs<runtime::Number>();
		auto rhs_num = rhs_obj.TryAs<runtime::Number>();
		if (lhs_num && rhs_num) {
			return ObjectHolder::Own(runtime::Number{ lhs_num->GetValue() + rhs_num->GetValue() });
		}

		auto lhs_class = lhs_obj.TryAs<runtime::ClassInstance>();
		if (lhs_class && lhs_class->HasMethod(ADD_METHOD, 1)) {
			return lhs_class->Call(ADD_METHOD, { rhs_obj }, context);
		}

		throw runtime_error("Addition is not implemented for these operands");
	}

	ObjectHolder Sub::Execute(Closure& closure, Context& context) {
		ObjectHolder lhs_obj = lhs_->Execute(closure, context);
		ObjectHolder rhs_obj = rhs_->Execute(closure, context);

		auto lhs_num = lhs_obj.TryAs<runtime::Number>();
		auto rhs_num = rhs_obj.TryAs<runtime::Number>();
		if (lhs_num && rhs_num) {
			return ObjectHolder::Own(runtime::Number{ lhs_num->GetValue() - rhs_num->GetValue() });
		}

		throw runtime_error("Subtraction is not implemented for these operands");
	}

	ObjectHolder Mult::Execute(Closure& closure, Context& context) {
		ObjectHolder lhs_obj = lhs_->Execute(closure, context);
		ObjectHolder rhs_obj = rhs_->Execute(closure, context);

		auto lhs_num = lhs_obj.TryAs<runtime::Number>();
		auto rhs_num = rhs_obj.TryAs<runtime::Number>();
		if (lhs_num && rhs_num) {
			return ObjectHolder::Own(runtime::Number{ lhs_num->GetValue() * rhs_num->GetValue() });
		}

		throw runtime_error("Multiplication is not implemented for these operands");
	}

	ObjectHolder Div::Execute(Closure& closure, Context& context) {
		ObjectHolder lhs_obj = lhs_->Execute(closure, context);
		ObjectHolder rhs_obj = rhs_->Execute(closure, context);

		auto lhs_num = lhs_obj.TryAs<runtime::Number>();
		auto rhs_num = rhs_obj.TryAs<runtime::Number>();
		if (lhs_num && rhs_num && rhs_num->GetValue() != 0) {
			return ObjectHolder::Own(runtime::Number{ lhs_num->GetValue() / rhs_num->GetValue() });
		}

		throw runtime_error("Division is not implemented for these operands");
	}

	ObjectHolder Compound::Execute(Closure& closure, Context& context) {
		for (auto& stmt : args_) {
			stmt->Execute(closure, context);
		}
		return ObjectHolder::None();
	}

	ObjectHolder Return::Execute(Closure& closure, Context& context) {
		ObjectHolder res_obj = statement_->Execute(closure, context);
		throw ExeptionWithObject(res_obj);
	}

	ClassDefinition::ClassDefinition(ObjectHolder cls)
		:cls_(move(cls))
	{
	}

	ObjectHolder ClassDefinition::Execute(Closure& closure, Context&) {
		string name = cls_.TryAs<runtime::Class>()->GetName();
		closure[name] = cls_;
		return cls_;
	}

	FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
		std::unique_ptr<Statement> rv)
		:object_(move(object)), field_name_(move(field_name)), rv_(move(rv))
	{
	}

	ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
		ObjectHolder obj = object_.Execute(closure, context);
		Closure& fields_closure = obj.TryAs<runtime::ClassInstance>()->Fields();
		fields_closure[field_name_] = rv_->Execute(closure, context);
		return fields_closure.at(field_name_);
	}

	IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
		std::unique_ptr<Statement> else_body)
		:condition_(move(condition)), if_body_(move(if_body)), else_body_(move(else_body))
	{
	}

	ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
		ObjectHolder condition_obj = condition_->Execute(closure, context);
		if (IsTrue(condition_obj)) {
			return if_body_->Execute(closure, context);
		}
		else if (else_body_) {
			return else_body_->Execute(closure, context);
		}
		else {
			return ObjectHolder::None();
		}
	}

	ObjectHolder Or::Execute(Closure& closure, Context& context) {
		ObjectHolder lhs_obj = lhs_->Execute(closure, context);
		ObjectHolder rhs_obj = rhs_->Execute(closure, context);

		if (lhs_obj && rhs_obj) {
			return ObjectHolder::Own(runtime::Bool{ IsTrue(lhs_obj) || IsTrue(rhs_obj) });
		}

		throw runtime_error("'Or' is not implemented for these operands");
	}

	ObjectHolder And::Execute(Closure& closure, Context& context) {
		ObjectHolder lhs_obj = lhs_->Execute(closure, context);
		ObjectHolder rhs_obj = rhs_->Execute(closure, context);

		if (lhs_obj && rhs_obj) {
			return ObjectHolder::Own(runtime::Bool{ IsTrue(lhs_obj) && IsTrue(rhs_obj) });
		}

		throw runtime_error("'And' is not implemented for these operands");
	}

	ObjectHolder Not::Execute(Closure& closure, Context& context) {
		ObjectHolder obj = arg_->Execute(closure, context);

		if (obj) {
			return ObjectHolder::Own(runtime::Bool{ !IsTrue(obj) });
		}

		throw runtime_error("'Not' is not implemented for this argument");
	}

	Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
		: BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(move(cmp)) {
	}

	ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
		ObjectHolder lhs_obj = lhs_->Execute(closure, context);
		ObjectHolder rhs_obj = rhs_->Execute(closure, context);
		return ObjectHolder::Own(runtime::Bool{ cmp_(lhs_obj, rhs_obj, context) });
	}

	NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
		:id_(NewInstanceId()), class__(class_), args_(move(args))
	{
	}

	NewInstance::NewInstance(const runtime::Class& class_)
		:id_(NewInstanceId()), class__(class_)
	{
	}

	ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
		string name = to_string(NewInstanceId()) + '_' + class__.GetName();

		runtime::ClassInstance class_instance(class__);
		closure[name] = ObjectHolder::Own(std::move(class_instance));

		auto* class_inst = const_cast<runtime::ClassInstance*>(closure.at(name).TryAs<runtime::ClassInstance>());
		if (class_inst->HasMethod(INIT_METHOD, args_.size())) {
			vector<ObjectHolder> args_executed(args_.size());
			auto it = args_.begin();
			for (ObjectHolder& arg_obj : args_executed) {
				arg_obj = move((*it)->Execute(closure, context));
				++it;
			}
			class_inst->Call(INIT_METHOD, args_executed, context);
		}
		return closure.at(name);
	}

	MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
		:body_(move(body))
	{
	}

	ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
		try {
			ObjectHolder result = body_->Execute(closure, context);
		}
		catch (ExeptionWithObject& exeption_with_obj) {
			return exeption_with_obj.obj_;
		}
		return ObjectHolder::None();
	}

}  // namespace ast