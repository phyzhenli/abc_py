# abc_py

Python interface for [abc](https://github.com/berkeley-abc/abc).


## Dependencies
```
boost
python3.8
```


## Build

```
git clone --recursive https://github.com/phyzhenli/abc_py.git
cd abc_py/abc
make ABC_USE_NO_READLINE=1 ABC_USE_STDINT_H=1 ABC_USE_PIC=1 libabc.a
cd ../
make
```


## Usage

```
from abc_py_ import AbcInterface_

abc = AbcInterface_()
abc.run('read s838.blif; st; ps')
abc.run('resyn2; ps')
```
or
```
from abc_py import AbcInterface

abc = AbcInterface()
abc.read('s838.blif')
abc.runcmd('ps; resyn2; ps')
```

Remember to set `PYTHONPATH` before running the python file.
