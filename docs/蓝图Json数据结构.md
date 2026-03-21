# 蓝图Json数据结构
## 蓝图、节点、变量包含关系
```mermaid
graph LR;
A[节点Node1]-->|执行流|B[节点Node2]
B[节点Node2]-->|执行流|C[节点Node3]
B[节点Node2]-->|数据流|C[节点Node3]
C[节点Node3]-->D[节点Node......]
```

这些节点、变量和流的集合被称为**蓝图**
## 具体数据结构
我们的蓝图以json格式保存和读写
### 节点Node

节点包含：唯一id，类型，引脚，属性

```json
"Nodes" : [
{
	"id" : 3,				//使用整数来表示id，是该蓝图内节点的唯一id
	"type" : "IntAddInt",	//整数加整数的加法函数
	"pins" : [		//具体引脚根据 节点类型 而定，下面的只是举例
	{"name" : "IEXEC","I/O" : "I","type" : "exec"},
	{"name" : "num1","I/O" : "I","type" : "int"},
	{"name" : "num2","I/O" : "I","type" : "int"},
	{"name" : "result","I/O" : "O","type" : "int"},
	{"name" : "OEXEC","I/O" : "O","type" : "exec"},
	],
	"properties" : {}	//节点自带的某些属性，比如我可以在这里加上 "num1" : 123,
},
//......
]
```
### 流Link
流负责连接2个引脚，故包含：源/目标节点id，源/目标引脚name
```json
"Links" : [
{
	"SourceNode" : 3,
	"SourcePin" : "result",
	"TargetNode" : 4,
	"TargetPin" : "num1"
},
//......
]
```
### 变量Variables
一个变量应该包含：变量名、变量类型、值

我们的引擎的变量类型只支持基本数据类型 (int,float,string,bool)
```json
"Variables" : [
{
	"name" : "my_number",
	"type" : "int",
	"value" : 456
},
{
	"name" : "my_name",
	"type" : "string",
	"value" : "MrSeagull"
},
//......
]
```
### 输入参数InParameters
当蓝图作为函数被另一个蓝图调用时，我们可能需要向其中传入参数
```json
"InParameters" : [
{
	"name" : "my_number",
	"type" : "int",
	"default_value" : 456		//当参数未传入时使用内置的默认参数
},
{
	"name" : "my_name",
	"type" : "string",
	"default_value" : "MrSeagull"
},
//......
]
```
### 输出参数OutParameters
当蓝图作为函数被另一个蓝图调用时，它可能需要向外传输参数
```json
"OutParameters" : [
{
	"name" : "my_number",
	"type" : "int",
	"default_value" : 456		//当参数未更新时使用内置的默认参数
},
{
	"name" : "my_name",
	"type" : "string",
	"default_value" : "MrSeagull"
},
//......
]
```
### 事件入口Event
事件入口是一个特殊的节点，它的数据结构与普通节点相比只是少了个入口引脚，由游戏虚拟机启动，需要加入特殊前缀到类型中以便识别
## 手写出一个蓝图
我们来写三个版本的蓝图来进行示例：在开始运行时打印4+5的和
### 方式1
创建2个变量，设置其初始值，并用加法函数计算两者的和，最后用打印函数输出其值
```json
{
	"Name" : "my_bp",
	"id" : 1,	//蓝图在整个项目中的唯一id，0被场景蓝图占据
	"Nodes" : [
	{
		"id" : 0,
		"type" : "EVENT_BeginPlay"
		"pins" : [
		{"name" : "OEXEC","I/O" : "O","type" : "exec"}
		]
	},
	{
		"id" : 1,
		"type" : "Add"	//无特殊前缀则被解释为普通函数
		"pins" : [
		{"name" : "IEXEC","I/O" : "I","type" : "exec"},
		{"name" : "value1","I/O" : "I","type" : "int"},
		{"name" : "value2","I/O" : "I","type" : "int"},
		{"name" : "result","I/O" : "O","type" : "int"},
		{"name" : "OEXEC","I/O" : "O","type" : "exec"},
		],
		"properties" : {}
	},
	{
		"id" : 2,
		"type" : "Print"	//无特殊前缀则被解释为普通函数
		"pins" : [
		{"name" : "IEXEC","I/O" : "I","type" : "exec"},
		{"name" : "content","I/O" : "I","type" : "int"},
		{"name" : "OEXEC","I/O" : "O","type" : "exec"},
		],
		"properties" : {}
	},
	{
		"id" : 3,
		"type" : "SET_VAR"	//表示 变量设置节点
		"pins" : [
		{"name" : "IEXEC","I/O" : "I","type" : "exec"},
		{"name" : "variable","I/O" : "I","type" : "int","literal" : "4"},		//literal表示字面值，在该节点被创建时默认为0，如果是字符串则还需要一对双引号包裹
		{"name" : "output","I/O" : "O","type" : ""},	//输出更改后的变量引用
		{"name" : "OEXEC","I/O" : "O","type" : "exec"},
		],
		"properties" : {
			"variable" : "a"	//必须设置，否则转译将报错；表示要设置的变量，设置后type将更新
		}
	},
	{
		"id" : 4,
		"type" : "SET_VAR"	//表示 变量设置节点
		"pins" : [
		{"name" : "IEXEC","I/O" : "I","type" : "exec"},
		{"name" : "variable","I/O" : "I","type" : "int","literal" : "5"},
		{"name" : "output","I/O" : "O","type" : ""},
		{"name" : "OEXEC","I/O" : "O","type" : "exec"},
		],
		"properties" : {
			"variable" : "b"	//必须设置，否则转译将报错；表示要设置的变量，设置后type将更新
		}
	},
	{
		"id" : 5,
		"type" : "GET_VAR"	//表示 变量引用获取节点
		"pins" : [
		{"name" : "output","I/O" : "O","type" : "int"}
		],
		"properties" : {
			"variable" : "a"	//必须设置，否则转译将报错；表示要获取的变量，设置后type将更新
		}
	},
	{
		"id" : 6,
		"type" : "GET_VAR"	//表示 变量引用获取节点
		"pins" : [
		{"name" : "output","I/O" : "O","type" : "int"}
		],
		"properties" : {
			"variable" : "b"	//必须设置，否则转译将报错；表示要获取的变量，设置后type将更新
		}
	}
	],
	"Variables" : [
	{
		"name" : "a",
		"type" : "int",
		"value" : 0
	},
	{
		"name" : "b",
		"type" : "int",
		"value" : 0
	}
	],
	"InParameters" : [
	{}
	],
	"OutParameters" : [
	{}
	],
	"Events" : [{
        "Event_Name" : "BeginPlay",
        "id" : 0	//对应节点id
    }]
	"Links" : [
	{	//beginplay --> set a
		"SourceNode" : 0,
		"SourcePin" : "OEXEC",
		"TargetNode" : 3,
		"TargetPin" : "IEXEC"
	},
	{	//set a --> set b
		"SourceNode" : 3,
		"SourcePin" : "OEXEC",
		"TargetNode" : 4,
		"TargetPin" : "IEXEC"
	},
    {	//set b --> add
		"SourceNode" : 4,
		"SourcePin" : "OEXEC",
		"TargetNode" : 1,
		"TargetPin" : "IEXEC"
	},
	{	//get a --> add
		"SourceNode" : 5,
		"SourcePin" : "output",
		"TargetNode" : 1,
		"TargetPin" : "value1"
	},
	{	//get b --> add
		"SourceNode" : 6,
		"SourcePin" : "output",
		"TargetNode" : 1,
		"TargetPin" : "value2"
	},
	{	//add --> print
		"SourceNode" : 1,
		"SourcePin" : "result",
		"TargetNode" : 2,
		"TargetPin" : "content"
	},
	]
}
```
### 方式2

创建2个变量的同时设置其初始值，并用加法函数计算两者的和，最后用打印函数输出其值

```json
{
	"Name" : "my_bp",
	"id" : 1,	//蓝图在整个项目中的唯一id，0被场景蓝图占据
	"Nodes" : [
	{
		"id" : 0,
		"type" : "EVENT_BeginPlay"
		"pins" : [
		{"name" : "OEXEC","I/O" : "O","type" : "exec"}
		]
	},
	{
		"id" : 1,
		"type" : "Add"	//无特殊前缀则被解释为普通函数
		"pins" : [
		{"name" : "IEXEC","I/O" : "I","type" : "exec"},
		{"name" : "value1","I/O" : "I","type" : "int"},
		{"name" : "value2","I/O" : "I","type" : "int"},
		{"name" : "result","I/O" : "O","type" : "int"},
		{"name" : "OEXEC","I/O" : "O","type" : "exec"},
		],
		"properties" : {}
	},
	{
		"id" : 2,
		"type" : "Print"	//无特殊前缀则被解释为普通函数
		"pins" : [
		{"name" : "IEXEC","I/O" : "I","type" : "exec"},
		{"name" : "content","I/O" : "I","type" : "int"},
		{"name" : "OEXEC","I/O" : "O","type" : "exec"},
		],
		"properties" : {}
	},
	{
		"id" : 3,
		"type" : "GET_VAR"	//表示 变量引用获取节点
		"pins" : [
		{"name" : "output","I/O" : "O","type" : "int"}
		],
		"properties" : {
			"variable" : "a"
		}
	},
	{
		"id" : 4,
		"type" : "GET_VAR"	//表示 变量引用获取节点
		"pins" : [
		{"name" : "output","I/O" : "O","type" : "int"}
		],
		"properties" : {
			"variable" : "b"
		}
	}
	],
	"Variables" : [
	{
		"name" : "a",
		"type" : "int",
		"value" : 4
	},
	{
		"name" : "b",
		"type" : "int",
		"value" : 5
	}
	],
	"InParameters" : [
	{}
	],
	"OutParameters" : [
	{}
	],
	"Events" : [{
        "Event_Name" : "BeginPlay",
        "id" : 0	//对应节点id
    }]
	"Links" : [
	{	//beginplay --> add
		"SourceNode" : 0,
		"SourcePin" : "OEXEC",
		"TargetNode" : 1,
		"TargetPin" : "IEXEC"
	},
	{	//get a --> add
		"SourceNode" : 3,
		"SourcePin" : "output",
		"TargetNode" : 1,
		"TargetPin" : "value1"
	},
	{	//get b --> add
		"SourceNode" : 4,
		"SourcePin" : "output",
		"TargetNode" : 1,
		"TargetPin" : "value2"
	},
	{	//add --> print
		"SourceNode" : 1,
		"SourcePin" : "result",
		"TargetNode" : 2,
		"TargetPin" : "content"
	},
	]
}
```

有时候我们会遇到以一个蓝图A为另一个蓝图B的节点的情况，此时就需要将节点类型改为 "BP_" + "B的名字"，蓝图解释器会自动查找并内联