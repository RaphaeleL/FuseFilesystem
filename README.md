# Fuse Filesystem  

## Information

Wenn die notwendige Arbeitsumgebung eingerichtet wurde, sollte sich das Template-Projekt korrekt übersetzen lassen und dann die Funktionalität des [_Simple & Stupid File Systems_](http://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/) bereitstellen.

## Start

```bash 
	make
	mkdir mount
	touch container.bin
	./mount.myfs container.bin log.txt mount
	cd mount
	ls
	cat file349
	cat file54
	cd ..
	fusermount -u mount
```

