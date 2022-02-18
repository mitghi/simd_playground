package main

import (
	"bufio"
	"os"
)

func main() {
	file, err := os.Create("./big.txt")
	if err != nil {
		panic(err)
	}
	defer file.Close()

	writer := bufio.NewWriter(file)

	for i := 0; i < 4400000000; i++ {
		writer.WriteString("tes,")
	}
	writer.Flush()
}
