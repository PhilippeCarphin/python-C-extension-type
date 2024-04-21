import mymodule

p = mymodule.Person()
print(p)

p = mymodule.Person("Guido", "van Rossum", number=80)
print(p)

p = mymodule.Person("Philippe", last_name="Carphin", number=99)
print(p)

p = mymodule.Person(first_name="Johnny")
print(p)
