from SCons.Script.SConscript import SConsEnvironment

def unit_test(env, source):
	test = env.Program(source)
	env.AddPostAction(test, './unit_tests/'+str(test[0]))
	return test

SConsEnvironment.unit_test = unit_test
