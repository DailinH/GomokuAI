## 2018/3/11
### DEBUG������ƿ����
* playout() -> 92.01%
  * simulate() -> 71.14%
    * getRandomMove() -> 40.65%
	* applyMove() -> 28.26%
  * reset() -> 18.69%
  * expand() -> 0.18%

* �ܼƣ�applyMove() -> 34.48%
  * checkMove()��DEBUG�棩 -> 2.18%
  * hashset[xxx] = xxx -> 6.90%
  * **hashset.erase() -> 11.43%**
  * **checkVictory() -> 13.79%**
    * ����ȫ�����������´����У�
	  ```cpp
		(x >= 0 && x < width) && (y >= 0 && y < height) 
        && (m_appliedMoves.count(pose) && m_appliedMoves[pose] == m_curPlayer);
	  ```

* �ܼƣ�getRandomMove() -> 41.38%
  * **hashset.bucket_size() -> 36.30%**
  * bucket = (bucket + 1) % hashset.bucket_count() -> 2.54%
  * hashset.begin(bucket), rand () % bucketSize -> 1.63


**�ܽ�**��DEBUGģʽ�£�STL�Ŀ�����Ϊ�˾��ԵĴ�ͷ��