// Copyright 2026 MrSeagull. All Rights Reserved.
#include"SharedTypes.h"
#include "GameVM.h"


Value calcBinary(const Value& a, const Value& b, char op) {
	if (a.type == ValueType::INT && b.type == ValueType::INT) {
		int x = a.i;
		int y = b.i;
		switch (op) {
		case '+':return Value::makeInt(x + y);
		case '-':return Value::makeInt(x - y);
		case'*':return Value::makeInt(x * y);
		case'/':return Value::makeInt(y != 0 ? x / y : 0.0f);
		}
	}
	float x = (a.type == ValueType::INT) ? a.i : a.f;
	float y = (b.type == ValueType::INT) ? b.i : b.f;
	switch (op) {
	case '+': return Value::makeFloat(x + y);
	case '-': return Value::makeFloat(x - y);
	case '*': return Value::makeFloat(x * y);
	case '/': return Value::makeFloat(y != 0 ? x / y : 0.0f);
	}
}

Value Node_Equal::compute(const Value& a, const Value& b) {
	if (a.type == ValueType::INT && b.type == ValueType::INT) {
		return Value::makeBool(a.i == b.i);
	}
	float fa = (a.type == ValueType::INT) ? a.i : a.f;
	float fb = (b.type == ValueType::INT) ? b.i : b.f;
	return Value::makeBool(fa == fb);
}
Value Node_Greater::compute(const Value& a, const Value& b) {

	// int > int
	if (a.type == ValueType::INT && b.type == ValueType::INT) {
		return Value::makeBool(a.i > b.i);
	}

	// 转 float 比较
	float fa = (a.type == ValueType::INT) ? a.i : a.f;
	float fb = (b.type == ValueType::INT) ? b.i : b.f;

	return Value::makeBool(fa > fb);
}
Value Node_Less::compute(const Value& a, const Value& b) {

	// int < int
	if (a.type == ValueType::INT && b.type == ValueType::INT) {
		return Value::makeBool(a.i < b.i);
	}

	float fa = (a.type == ValueType::INT) ? a.i : a.f;
	float fb = (b.type == ValueType::INT) ? b.i : b.f;

	return Value::makeBool(fa < fb);
}

Value Node_ADD::compute(const Value& a, const Value& b) {

	// int + int
	if (a.type == ValueType::INT && b.type == ValueType::INT) {
		return Value::makeInt(a.i + b.i);
	}

	// float 参与 → float
	if ((a.type == ValueType::FLOAT || b.type == ValueType::FLOAT)) {
		float fa = (a.type == ValueType::INT) ? a.i : a.f;
		float fb = (b.type == ValueType::INT) ? b.i : b.f;
		return Value::makeFloat(fa + fb);
	}

	// string 拼接
	if (a.type == ValueType::STRING && b.type == ValueType::STRING) {
		return Value::makeString(a.s + b.s);
	}

	OutputDebugStringA("Add type error\n");
	return Value();
}
Value Node_Sub::compute(const Value& a, const Value& b) {

	if (a.type == ValueType::INT && b.type == ValueType::INT) {
		return Value::makeInt(a.i - b.i);
	}

	float fa = (a.type == ValueType::INT) ? a.i : a.f;
	float fb = (b.type == ValueType::INT) ? b.i : b.f;
	return Value::makeFloat(fa - fb);
}
Value Node_Mul::compute(const Value& a, const Value& b) {
	if (a.type == ValueType::STRING && b.type == ValueType::INT) {
		std::string res;
		for (int i = 0; i < b.i; i++) {
			res += a.s;
		}
		return Value::makeString(res);
	}
	if (a.type == ValueType::INT && b.type == ValueType::INT) {
		return Value::makeInt(a.i * b.i);
	}
	float fa = (a.type == ValueType::INT) ? a.i : a.f;
	float fb = (b.type == ValueType::INT) ? b.i : b.f;
	return Value::makeFloat(fa * fb);
}
Value Node_Div::compute(const Value& a, const Value& b) {
	float fb = (b.type == ValueType::INT) ? b.i : b.f;

	if (fb == 0.0f) {
		OutputDebugStringA("Divide by zero!\n");
		return Value();
	}

	float fa = (a.type == ValueType::INT) ? a.i : a.f;

	return Value::makeFloat(fa / fb);
}

void SetTransforNode::func_for_VM(ExecutionContext& ctx) {
    ObjectData* obj = GetObjByName(targetName);
	if ((obj == nullptr) || (!obj->Transform.has_value())) return;
	auto& tf = obj->Transform.value();
	tf.Location.x = in_loc_x == nullptr ? GetValueByAnalyzeLiteral(literals[0]).f : in_loc_x->f;
	tf.Location.y = in_loc_y == nullptr ? GetValueByAnalyzeLiteral(literals[1]).f : in_loc_y->f;
	tf.Location.z = in_loc_z == nullptr ? GetValueByAnalyzeLiteral(literals[2]).i : in_loc_z->i;

	tf.Rotation.r = in_rotation == nullptr ? GetValueByAnalyzeLiteral(literals[3]).f : in_rotation->f;

	tf.Scale.x = in_scale_x == nullptr ? GetValueByAnalyzeLiteral(literals[4]).f : in_scale_x->f;
	tf.Scale.y = in_scale_y == nullptr ? GetValueByAnalyzeLiteral(literals[5]).f : in_scale_y->f;
}
ObjectData* SetTransforNode::GetObjByName(const std::string& name) {
	if (!level) return nullptr;
	if (name == "") return owner;

	// 使用递归辅助 lambda
	std::function<ObjectData* (ObjectData*)> findRecursive =
		[&](ObjectData* obj) -> ObjectData* {
		if (!obj) return nullptr;
		if (obj->name == name) return obj;
		for (auto& [childName, child] : obj->objects) {
			ObjectData* found = findRecursive(child);
			if (found) return found;
		}
		return nullptr;
		};

	// 遍历场景根对象
	for (auto& [objName, obj] : level->objects) {
		ObjectData* found = findRecursive(obj);
		if (found) return found;
	}
	return nullptr;
}

Value BinaryOpNode::GetValueByAnalyzeLiteral(const std::string& literal)
{
	if (literal.empty())
		return Value();                     // 空字面量 → NONE

	// 1. bool
	if (literal == "true")  return Value::makeBool(true);
	if (literal == "false") return Value::makeBool(false);

	// 2. int
	auto allDigits = [](const std::string& s, size_t start, size_t end) {
		for (size_t i = start; i < end; ++i)
			if (!std::isdigit(static_cast<unsigned char>(s[i])))
				return false;
		return true;
		};

	size_t idx = 0;
	if (literal[idx] == '+' || literal[idx] == '-') {
		++idx;
		if (idx == literal.size())          // 只有一个符号，不是数字
			return Value::makeString(literal);
	}
	if (allDigits(literal, idx, literal.size())) {
		// 禁止前导零，除非数字部分就是 "0"
		if (literal[idx] != '0' || (literal.size() - idx) == 1)
			return Value::makeInt(std::stoi(literal));
	}

	// 3. float（必须包含小数点，且至少有一位数字，末尾可选 f）
	if (literal.find('.') != std::string::npos) {
		size_t pos = 0;
		if (literal[pos] == '+' || literal[pos] == '-')
			++pos;

		bool hasDigit = false;
		bool dotSeen = false;
		bool valid = true;

		for (; pos < literal.size(); ++pos) {
			char c = literal[pos];
			if (std::isdigit(static_cast<unsigned char>(c))) {
				hasDigit = true;
			}
			else if (c == '.') {
				if (dotSeen) { valid = false; break; }
				dotSeen = true;
			}
			else if (c == 'f') {                   // 小写 f
				if (pos != literal.size() - 1) {     // f 只能在末尾
					valid = false;
					break;
				}
				// 有 f 时前面也必须已经出现过小数点（dotSeen 保证）
			}
			else {
				valid = false;
				break;
			}
		}

		if (valid && hasDigit && dotSeen)
			return Value::makeFloat(std::stof(literal));
	}

	// 4. 以上都不满足 → 字符串
	return Value::makeString(literal);
}
Value SetTransforNode::GetValueByAnalyzeLiteral(const std::string& literal)
{
	if (literal.empty())
		return Value();                     // 空字面量 → NONE

	// 1. bool
	if (literal == "true")  return Value::makeBool(true);
	if (literal == "false") return Value::makeBool(false);

	// 2. int
	auto allDigits = [](const std::string& s, size_t start, size_t end) {
		for (size_t i = start; i < end; ++i)
			if (!std::isdigit(static_cast<unsigned char>(s[i])))
				return false;
		return true;
		};

	size_t idx = 0;
	if (literal[idx] == '+' || literal[idx] == '-') {
		++idx;
		if (idx == literal.size())          // 只有一个符号，不是数字
			return Value::makeString(literal);
	}
	if (allDigits(literal, idx, literal.size())) {
		// 禁止前导零，除非数字部分就是 "0"
		if (literal[idx] != '0' || (literal.size() - idx) == 1)
			return Value::makeInt(std::stoi(literal));
	}

	// 3. float（必须包含小数点，且至少有一位数字，末尾可选 f）
	if (literal.find('.') != std::string::npos) {
		size_t pos = 0;
		if (literal[pos] == '+' || literal[pos] == '-')
			++pos;

		bool hasDigit = false;
		bool dotSeen = false;
		bool valid = true;

		for (; pos < literal.size(); ++pos) {
			char c = literal[pos];
			if (std::isdigit(static_cast<unsigned char>(c))) {
				hasDigit = true;
			}
			else if (c == '.') {
				if (dotSeen) { valid = false; break; }
				dotSeen = true;
			}
			else if (c == 'f') {                   // 小写 f
				if (pos != literal.size() - 1) {     // f 只能在末尾
					valid = false;
					break;
				}
				// 有 f 时前面也必须已经出现过小数点（dotSeen 保证）
			}
			else {
				valid = false;
				break;
			}
		}

		if (valid && hasDigit && dotSeen)
			return Value::makeFloat(std::stof(literal));
	}

	// 4. 以上都不满足 → 字符串
	return Value::makeString(literal);
}

bool PlaySound_Node::getBOOLfromLiteral(std::string s) {
	if (s == "TRUE" || s == "true")  return true;
	// 其他情况（包括 "FALSE", "false", 空串, 任意其他字符串）均返回 false
	return false;
}

float PlaySound_Node::getFLOATfromLiteral(std::string s) {
	// 空字符串 -> 1.0f
	if (s.empty()) return 1.0f;

	// 1. 只能包含数字、小数点、正负号、f/F
	for (char c : s) {
		if (!(std::isdigit(c) || c == '.' || c == '+' || c == '-' || c == 'f' || c == 'F')) {
			return 1.0f;
		}
	}

	// 2. 处理可选的前导符号
	size_t start = 0;
	if (s[0] == '+' || s[0] == '-') {
		start = 1;
		if (s.size() == 1) return 1.0f; // 单独符号
	}

	// 3. 检查结尾的 f/F
	bool hasF = false;
	size_t end = s.size();
	if (s.back() == 'f' || s.back() == 'F') {
		hasF = true;
		end = s.size() - 1;
		if (end == start) return 1.0f; // 例如 "f"、"+f"
	}

	// 提取数字部分（去掉符号和 f）
	std::string numPart = s.substr(start, end - start);
	if (numPart.empty()) return 1.0f;

	// 4. 检查小数点
	int dotCount = 0;
	size_t dotPos = std::string::npos;
	for (size_t i = 0; i < numPart.size(); ++i) {
		if (numPart[i] == '.') {
			dotCount++;
			dotPos = i;
		}
	}
	if (dotCount > 1) return 1.0f;                     // 多个小数点
	if (dotCount == 1 && dotPos == numPart.size() - 1) {
		return 1.0f;                                   // 小数没有小数位（例如 "0."）
	}

	// 5. 找到第一个数字（用于判断前导零）
	char firstDigit = 0;
	for (char c : numPart) {
		if (c >= '0' && c <= '9') {
			firstDigit = c;
			break;
		}
	}
	if (firstDigit == 0) return 1.0f; // 没有数字（理论上不会执行，因为前面已确保非空）

	// 6. 转换为浮点数
	float value;
	try {
		value = std::stof(numPart);
	}
	catch (...) {
		return 1.0f;   // 转换失败（如溢出）
	}

	// 7. 前导零规则：值不为0且第一位数字是'0'时返回 1.0f
	if (value != 0.0f && firstDigit == '0') {
		return 1.0f;
	}

	return value;
}