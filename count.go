package main

import (
	"fmt"
	"log"
	"os"
	"sync"
	"sync/atomic"
	"time"
)

func main() {
	var (
		fpath string
	)
	if len(os.Args) < 2 {
		panic("first argument filepath is missing")
	}
	fpath = os.Args[1]
	start := time.Now()
	content, err := os.ReadFile(fpath)
	if err != nil {
		panic(err)
	}
	elapsed := time.Since(start)
	fmt.Printf("[+] file loaded ( took %s )", elapsed)
	countNaive(content)
	countPool(content)
}

func countNaive(content []byte) (result int) {
	start := time.Now()

	for _, ch := range content {
		if ch == ',' {
			result += 1
		}
	}

	elapsed := time.Since(start)
	log.Printf("countNaive: took %s, count: %d commas", elapsed, result)
	return result
}

func countPool(content []byte) (result int64) {
	start := time.Now()

	group := 16
	ln := len(content)
	chsize := ln / group
	wg := sync.WaitGroup{}

	for i := 0; i < group; i++ {
		wg.Add(1)
		go func(index int, chunk int) {
			defer wg.Done()
			start := index * chunk
			end := start + chunk
			if start >= ln {
				return
			}
			if end >= ln {
				end = ln - 1
			}
			count := 0
			if index == group-1 {
				for _, ch := range content[start:] {
					if ch == ',' {
						count += 1
					}
				}
			} else {
				for _, ch := range content[start:end] {
					if ch == ',' {
						count += 1
					}
				}
			}
			atomic.AddInt64(&result, int64(count))
		}(i, chsize)
	}
	wg.Wait()

	elapsed := time.Since(start)
	log.Printf("countPool: took %s, count: %d commas", elapsed, result)
	return result
}
